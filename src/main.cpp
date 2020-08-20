#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <SPI.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "website.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t temprature_sens_read();
//uint8_t g_phyFuns;

#ifdef __cplusplus
}
#endif

struct measure {
  float avg = 0;
  float max = 0;
  float min = 0;
};
struct data
{
  measure temperature;
  measure humidity;
};


int state = 0; // 0 = initial mode - no wifi connection, 1 = AP mode - connected, 2 = client mode - connected
int failedReads = 0;
String ssid = "";
String pass = "";
String server = "";
String topic = "";
String mqttuser = "";
String mqttpass = "";
int interval = 5;   // data transmission intervall, in seconds
int sensors = 3;   // number of connected sensors
#define OUTPUTBUFFERSIZE 256
char outputBuffer[OUTPUTBUFFERSIZE];

const int capacity = JSON_ARRAY_SIZE(3) + 3*JSON_OBJECT_SIZE(2) + 6*JSON_OBJECT_SIZE(3);
StaticJsonDocument<capacity> doc;

float temp[3][10] = {{0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0}};
float humid[3][10] = {{0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0}};
bool dhtActive[3] = {false,false,false};
bool dhtFailedRead[3] = {false,false,false};
long dhtReReadTime = 0;
int dataIndex = 0;
#define DATALENGTH 10
#define DHTSENSORS 3
#define DHTDATAPIN1 18
#define DHTDATAPIN2 19
#define DHTDATAPIN3 21

// ESP AP mode settings
const char *APssid = "ESP32AP1";
const char *APpwd = "esp32ap1";

// MQTT
char server_c[64] = "";
char topic_c[128] = "";
int port = 1883;

// TIMING
long lastReconnectAttempt = 0;
long lastDataReadEvent = 0;
#define SENSORCHECKINTERVALL 60000
long lastSensorCheckEvent = SENSORCHECKINTERVALL;


WiFiServer httpserver(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht[] = {{DHTDATAPIN1, DHT22},{DHTDATAPIN2, DHT22},{DHTDATAPIN3, DHT22}};

// declare functions
void publish(char* subtopic);
void publish(char* subtopic, bool sendError);
bool mqttReconnect();


// define functions
void readMemory(){
  int address = 0;
  Serial.println("FLASH MEMORY DATA");
  Serial.println("-----------------");
  Serial.print("SSID: ");
  ssid = EEPROM.readString(address);
  address = address + ssid.length() + 1;
  Serial.println(ssid);

  Serial.print("PASS: ");
  pass = EEPROM.readString(address);
  address = address + pass.length() + 1;
  Serial.println(pass);

  Serial.print("Server: ");
  server = EEPROM.readString(address);
  address = address + server.length() + 1;
  Serial.println(server);

  Serial.print("Topic: ");
  topic = EEPROM.readString(address);
  address = address + topic.length() + 1;
  Serial.println(topic);

  Serial.print("MqttUser: ");
  mqttuser = EEPROM.readString(address);
  address = address + mqttuser.length() + 1;
  Serial.println(mqttuser);

  Serial.print("MqttPass: ");
  mqttpass = EEPROM.readString(address);
  address = address + mqttpass.length() + 1;
  Serial.println(mqttpass);
  
  Serial.print("Interval: ");
  interval = EEPROM.readInt(address);
  if(interval < 0){
    interval = 5;
  }
  address = address + sizeof interval;
  Serial.println(interval);

  Serial.print("Sensors: ");
  sensors = EEPROM.readInt(address);
  if(sensors < 0){
    sensors = 3;
  }
  address = address + sizeof sensors;
  Serial.println(sensors);

  Serial.println();
}

void removeURLEncoding(String* str){
  for(int i = 0; i < str->length(); i++){
    if(str->charAt(i) == '+'){
      str->setCharAt(i,' ');
    }
    else if(str->charAt(i) == '%'){
      char c = ' ';
      String hex = str->substring(i+1,i+3);
      if(hex == "2F") // '/' = Hex 2F = Dec 47
        c = '/';
      else if(hex == "3A")  // ':' = Hex 3A = Dec 58
        c = ':';
      
      *str = str->substring(0,i) + c + str->substring(i+3);
    }
  }
}

String getFormParam(String param, String* req){
  int strStart = req->indexOf(param+'=') + param.length() + 1;
  int strEnd = req->indexOf("&",strStart);
  if(strEnd == -1)
    strEnd = req->length();
  
  String str = req->substring(strStart, strEnd);
  removeURLEncoding(&str);
  return str;
}

bool writeToFlash(int* address, String* str, String* current, bool write){
  if(str->length() > 0){
    EEPROM.writeString(*address,*str);
    *address = *address + str->length() + 1 ;
    return true;
  }
  else if (write){
    EEPROM.writeString(*address,*current);
    *address = *address + current->length() + 1;
    return true;
  }
  else  {
    *address = *address + current->length() + 1;
    return write;
  }
}

bool writeToFlash(int* address, int* value, int* current, bool write){
  Serial.printf("Writing int: %i\n", *value);
  if (write || *value != *current){
    *address = *address + EEPROM.writeInt(*address,*value);
    return true;
  }
  else  {
    *address = *address + 4;
    return write;
  }
}

void writeMemory(String *req){
  String _ssid = getFormParam("ssid", req);
  String _pass = getFormParam("password", req);
  String _server = getFormParam("server", req);
  String _topic = getFormParam("topic", req);
  String _mqttuser = getFormParam("mqttuser", req);
  String _mqttpass = getFormParam("mqttpass", req);
  int _interval = getFormParam("interval", req).toInt();
  int _sensors = getFormParam("sensors", req).toInt();

  //save to flash
  int address = 0;
  bool write = false;

  write = writeToFlash(&address, &_ssid, &ssid, write);
  write = writeToFlash(&address, &_pass, &pass, write);
  write = writeToFlash(&address, &_server, &server, write);
  write = writeToFlash(&address, &_topic, &topic, write);
  write = writeToFlash(&address, &_mqttuser, &mqttuser, write);
  write = writeToFlash(&address, &_mqttpass, &mqttpass, write);
  write = writeToFlash(&address, &_interval, &interval, write);
  write = writeToFlash(&address, &_sensors, &sensors, write);
  
  Serial.printf("max Address: %i\n", address);

  EEPROM.commit();

  Serial.printf("ssid: %s\n",_ssid.c_str());
  Serial.printf("pass: %s\n",_pass.c_str());
  Serial.printf("server: %s\n",_server.c_str());
  Serial.printf("topic: %s\n",_topic.c_str());
  Serial.printf("mqtt user: %s\n",_mqttuser.c_str());
  Serial.printf("mqtt pass: %s\n",_mqttpass.c_str());
  Serial.printf("interval: %i\n",_interval);
  Serial.printf("sensors: %i\n",_sensors);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  if(!EEPROM.begin(256)){
    Serial.println("Error initializing eeprom!");
    Serial.println("Restarting...");
    ESP.restart();
  }
  Serial.println();
  
  readMemory();

  // try to connect to wifi network
  if(ssid.length() > 1 && pass.length() > 1){
    WiFi.begin(ssid.c_str(),pass.c_str());
    for(int i = 0; i < 15; i++){
      if(WiFi.waitForConnectResult() != WL_CONNECTED){
        WiFi.disconnect();
        delay(10);
        Serial.printf("WifiStatus: %i ", WiFi.status());
        WiFi.begin(ssid.c_str(),pass.c_str());
        Serial.println("Connecting...");
        delay(1000);
      }
      else{
        Serial.println("Connected to Wifi");
        Serial.print("Local IP: ");
        Serial.println(WiFi.localIP());
        int s = server.lastIndexOf(':');

        if(mqttClient.setBufferSize(320))
          Serial.println("MQTT client buffer increased");
        else
          Serial.println("MQTT client buffer increase failed");

        server.substring(0,s).toCharArray(server_c,64);
        topic.toCharArray(topic_c,128);
        port = server.substring(s+1).toInt();
        mqttClient.setServer(server_c, port);
        mqttReconnect();

        pinMode(DHTDATAPIN1, INPUT);
        pinMode(DHTDATAPIN2, INPUT);
        pinMode(DHTDATAPIN3, INPUT);
        
        /* begin DHT sensors and check if active */
        for (int i = 0; i < sensors; i++)
        {
          dht[i].begin();
          if(!isnan(dht[i].readTemperature())){
            dhtActive[i] = true;
          }
          Serial.print("Sensor active? ");
          Serial.println(dhtActive[i]);
        }

        if(mqttClient.connected()){
          sprintf(outputBuffer, "{\"info\": \"Device startup, number of sensors set to: %i, sensor status:  1: %s, 2: %s, 3: %s\"}", sensors, dhtActive[0] ? "ok" : "n/c", dhtActive[1] ? "ok" : "n/c",dhtActive[2] ? "ok" : "n/c");
          publish((char*)"diag");
        }
        i = 15;
        state = 2;
      }
    }
  }

  // start access point
  if(state == 0){
    WiFi.disconnect();
    Serial.println("Starting access point...");
    if(WiFi.softAP(APssid, APpwd)){
      delay(100);
      WiFi.setTxPower(WIFI_POWER_7dBm);
      //started
      Serial.print("TX power: ");
      Serial.println(WiFi.getTxPower());
      
      Serial.print("IP address: ");
      Serial.println(WiFi.softAPIP());

      state = 1;
    }
  }

  if(state != 0){
    httpserver.begin();
    Serial.println("http server started");
  }
  else{
    Serial.println("Was not able to connect to wifi network or start access point, restarting...");
    delay(10000);
    ESP.restart();
  }
}

double getMemUsage(){
  return (double)ESP.getFreeHeap()/(double)ESP.getHeapSize();
}

void handleHttpClient(WiFiClient* client){
  if(client->connected()){
        String req = client->readString();
        Serial.println(req);
        if(req.substring(0, 6) == "GET / "){
          Serial.println("requested root");
          renderPage(client, "root");
        }
        else if(req.substring(0, 12) == "GET /signal "){
          renderPage(client);
          client->print(WiFi.RSSI());
          Serial.println(WiFi.RSSI());
        }
        else if(req.substring(0, 13) == "GET /restart "){
          renderPage(client, "noContent");
          ESP.restart();
        }
        else if(req.substring(0, 13) == "POST /submit "){
          Serial.println("requested submit");

          renderPage(client, "root");

          writeMemory(&req);

          client->stop();
          delay(1000);

          Serial.println("Restarting...");
          WiFi.disconnect();
          ESP.restart();
        }
      }
      client->stop();
}

bool mqttReconnect(){
  Serial.print("Attempting MQTT connection...");
  // Create a random client ID
  String clientId = "ESP32Client-";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect
  if (mqttClient.connect(clientId.c_str(),mqttuser.c_str(),mqttpass.c_str())) {
    Serial.println("connected");
    
    //topic.toCharArray(topic_c,128);
    //sprintf(topic_c, "%s/%s", topic.c_str(), "debug");
    //Serial.println(mqttClient.subscribe("debug"));
  
  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
  }

  return mqttClient.connected();
}

void stat(data d[DHTSENSORS], float (&tempArray)[3][10], float (&humidArray)[3][10]){
  for (int i = 0; i < DHTSENSORS; i++)
  {
    if(dhtActive[i]){
      d[i].temperature.avg = tempArray[i][0];
      d[i].temperature.max = tempArray[i][0];
      d[i].temperature.min = tempArray[i][0];
      d[i].humidity.avg = humidArray[i][0];
      d[i].humidity.max = humidArray[i][0];
      d[i].humidity.min = humidArray[i][0];

      
      
      for(int j = 1; j < DATALENGTH; j++){
        d[i].temperature.avg = d[i].temperature.avg + tempArray[i][j];
        d[i].humidity.avg = d[i].humidity.avg + humidArray[i][j];
        if(tempArray[i][j] < d[i].temperature.min)
          d[i].temperature.min = tempArray[i][j];
        if(tempArray[i][j] > d[i].temperature.max)
          d[i].temperature.max = tempArray[i][j];
        if(humidArray[i][j] < d[i].humidity.min)
          d[i].humidity.min = humidArray[i][j];
        if(humidArray[i][j] > d[i].humidity.max)
          d[i].humidity.max = humidArray[i][j];
      }
      d[i].temperature.avg = d[i].temperature.avg/DATALENGTH;
      d[i].humidity.avg = d[i].humidity.avg/DATALENGTH;
    }
  }
}

float roundto1(float f){
  f = f*10;
  f = round(f);
  
  return f/10;
}

void makeJson(data d[DHTSENSORS]){
  for(int i = 0; i < sensors; i++){
    if(dhtActive[i]){
      doc[i]["t"]["a"] = roundto1(d[i].temperature.avg);
      doc[i]["t"]["ma"] = roundto1(d[i].temperature.max);
      doc[i]["t"]["mi"] = roundto1(d[i].temperature.min);
      doc[i]["h"]["a"] = roundto1(d[i].humidity.avg);
      doc[i]["h"]["ma"] = roundto1(d[i].humidity.max);
      doc[i]["h"]["mi"] = roundto1(d[i].humidity.min);
    }
    else{
      doc[i] = serialized("{}");
    }
  }
  serializeJson(doc,outputBuffer,OUTPUTBUFFERSIZE);
}

void publishErrorMessage(char* str){
  sprintf(outputBuffer, "{\"error\": \"%s\"}", str);
  publish((char*)"diag", false);
}

void publish(char* subtopic){
  publish(subtopic,true);
}

void publish(char* subtopic, bool sendError){
  topic.toCharArray(topic_c,128);
  sprintf(topic_c, "%s/%s", topic.c_str(), subtopic);

  if(mqttClient.publish(topic_c, outputBuffer)){
    Serial.println("Published");
  }
  else
  {
    Serial.println("Failed to publish");
    publishErrorMessage((char*)"Failed to publish mqtt message.");
  }
}

bool readSensors(bool readAll = true){
  bool result = true;
  long now = millis();
  for(int i = 0; i < sensors; i++){ 
    if(dhtActive[i] && (dhtFailedRead[i] || readAll)){
      temp[i][dataIndex] = dht[i].readTemperature();
      humid[i][dataIndex] = dht[i].readHumidity();
      
      if(isnan(temp[i][dataIndex])){
        result = false;
        dhtFailedRead[i] = true;
        dhtReReadTime = now + 3000; // re read after 3sec
      }
    }
  }
  return result;
}

void checkReadFailure(){
  for(int i = 0; i < sensors; i++){
    if(dhtActive[i]){
      if(isnan(temp[i][dataIndex])){
        failedReads++;
        sprintf(outputBuffer, "{\"error\": \"Sensor %i read error.\"}",i);
        publish((char*)"diag");
        Serial.printf("failedReads: %i\n", failedReads);
      }
    }
  }
}

void checkSensorStatus(){
  for(int i = 0; i < sensors; i++){
    if(!dhtActive[i]){
      sprintf(outputBuffer, "{\"error\": \"Sensor check failed (sensor: %i not active), restarting...\"}",i+1);
      publish((char*)"diag");

      Serial.println("Restarting because sensor is offline...");
      delay(1000);
      ESP.restart();
    }
  }
}

void loop() {
  if(state != 0){
    WiFiClient client = httpserver.available();
    if(client){
      handleHttpClient(&client);
    }
    if(state == 2){
      long now = millis();
      if(!mqttClient.connected()){
        if (now - lastReconnectAttempt > 5000) {
          lastReconnectAttempt = now;
          // Attempt to reconnect
          if (mqttReconnect()) {
            lastReconnectAttempt = 0;
          }
        }
      }
      else{

        // try to re-read sensor if failed
        if(dhtReReadTime > 0 && now > dhtReReadTime){
          dhtReReadTime = 0;
          if(readSensors(false)){
            sprintf(outputBuffer, "{\"info\": \"Sensor re-reading successful.\"}");
          }
          else{
            sprintf(outputBuffer, "{\"error\": \"Sensor re-reading failed.\"}");
          }
          
          publish((char*)"diag");
        }

        // read sensors and send data
        if (now - lastDataReadEvent > (interval * 1000) / DATALENGTH ) {
          lastDataReadEvent = now;
          
          if(readSensors()){
            checkReadFailure();
          }

          dataIndex++;
          if(dataIndex >= DATALENGTH){
            Serial.println("Data index reset");
            dataIndex = 0;

            data d[DHTSENSORS];
            stat(d, temp, humid);
            makeJson(d);

            publish((char*)"data");
            delay(1000);

            sprintf(outputBuffer, "{\"rssi\": %d,\"heap\": %f, \"temp\": %f}", WiFi.RSSI(),getMemUsage(), temperatureRead());
            publish((char*)"diag");
          }

        }

        if (now > lastSensorCheckEvent){
          lastSensorCheckEvent = now + SENSORCHECKINTERVALL;
          checkSensorStatus();
        }
      }
    }
  }

  if(failedReads >= 30){
    sprintf(outputBuffer, "{\"error\": \"Sensor read error (failedReads: %i), restarting...\"}",failedReads);
    publish((char*)"diag");
    Serial.println("Restarting because of reading errors...");
    delay(1000);
    ESP.restart();
  }
}