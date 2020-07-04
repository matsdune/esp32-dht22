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
String ssid = "";
String pass = "";
String server = "";
String topic = "";
String mqttuser = "";
String mqttpass = "";
char outputBuffer[256];

const int capacity = JSON_ARRAY_SIZE(3) + 3*JSON_OBJECT_SIZE(2) + 6*JSON_OBJECT_SIZE(3);
StaticJsonDocument<capacity> doc;

float temp[3][10] = {{0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0}};
float humid[3][10] = {{0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0}};
bool dhtActive[3] = {false,false,false};
int dataIndex = 0;
#define DATALENGTH 10
#define DHTSENSORS 3
#define DHTDATAPIN1 18
#define DHTDATAPIN2 19
#define DHTDATAPIN3 21

const char *APssid = "ESP32AP1";
const char *APpwd = "esp32ap1";

char server_c[64] = "";
char topic_c[128] = "";
int port = 1883;
long lastReconnectAttempt = 0;
long lastDataSendEvent = 0;


WiFiServer httpserver(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht[] = {{DHTDATAPIN1, DHT22},{DHTDATAPIN2, DHT22},{DHTDATAPIN3, DHT22}};

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

void writeMemory(String *req){
  String _ssid = getFormParam("ssid", req);
  String _pass = getFormParam("password", req);
  String _server = getFormParam("server", req);
  String _topic = getFormParam("topic", req);
  String _mqttuser = getFormParam("mqttuser", req);
  String _mqttpass = getFormParam("mqttpass", req);

  //save to flash
  int address = 0;
  bool write = false;

  write = writeToFlash(&address, &_ssid, &ssid, write);
  write = writeToFlash(&address, &_pass, &pass, write);
  write = writeToFlash(&address, &_server, &server, write);
  write = writeToFlash(&address, &_topic, &topic, write);
  write = writeToFlash(&address, &_mqttuser, &mqttuser, write);
  write = writeToFlash(&address, &_mqttpass, &mqttpass, write);

  EEPROM.commit();

  Serial.print("ssid: ");
  Serial.println(_ssid);
  Serial.print("pass: ");
  Serial.println(_pass);
  Serial.print("server: ");
  Serial.println(_server);
  Serial.print("topic: ");
  Serial.println(_topic);
  Serial.print("mqtt user: ");
  Serial.println(_mqttuser);
  Serial.print("mqtt pass: ");
  Serial.println(_mqttpass);
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

        server.substring(0,s).toCharArray(server_c,64);
        topic.toCharArray(topic_c,128);
        port = server.substring(s+1).toInt();
        mqttClient.setServer(server_c, port);
        pinMode(DHTDATAPIN1, INPUT);
        pinMode(DHTDATAPIN2, INPUT);
        pinMode(DHTDATAPIN3, INPUT);
        
        /* begin DHT sensors and chec if active */
        for (int i = 0; i < DHTSENSORS; i++)
        {
          dht[i].begin();
          if(!isnan(dht[i].readTemperature())){
            dhtActive[i] = true;
          }
          Serial.print("Sensor active? ");
          Serial.println(dhtActive[i]);
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

void handleHttpClient(WiFiClient* client){
  if(client->connected()){
        String req = client->readString();
        Serial.println(req);
        if(req.substring(0, 6) == "GET / "){
          Serial.println("requested root");

          client->print(header);
          client->print(root);
        }
        else if(req.substring(0, 12) == "GET /signal "){
          client->print(header);
          client->print(WiFi.RSSI());
          Serial.println(WiFi.RSSI());
        }
        else if(req.substring(0, 13) == "POST /submit "){
          Serial.println("requested submit");

          client->print(header);
          client->print(root);

          writeMemory(&req);

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

void makeJson(data* d){
  for(int i = 0; i < DHTSENSORS; i++){
    if(dhtActive[i]){
      doc[i]["t"]["a"] = d->temperature.avg;
      doc[i]["t"]["ma"] = d->temperature.max;
      doc[i]["t"]["mi"] = d->temperature.min;
      doc[i]["h"]["a"] = d->humidity.avg;
      doc[i]["h"]["ma"] = d->humidity.max;
      doc[i]["h"]["mi"] = d->humidity.min;
    }
    else{
      doc[i] = serialized("{}");
    }
  }
  serializeJson(doc,outputBuffer,256);
}

void publish(char* subtopic){
  topic.toCharArray(topic_c,128);
  sprintf(topic_c, "%s/%s", topic.c_str(), subtopic);
  mqttClient.publish(topic_c, outputBuffer);
}

double getMemUsage(){
  return (double)ESP.getFreeHeap()/(double)ESP.getHeapSize();
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
        if (now - lastDataSendEvent > 5000) {
          lastDataSendEvent = now;
          
          for(int i = 0; i < DHTSENSORS; i++){
            temp[i][dataIndex] = dht[i].readTemperature();
            humid[i][dataIndex] = dht[i].readHumidity();
          }
          dataIndex++;
          if(dataIndex >= DATALENGTH){
            Serial.println("Data index reset");
            dataIndex = 0;

            data d[DHTSENSORS];
            stat(d, temp, humid);
            makeJson(d);

            publish((char*)"data");

            sprintf(outputBuffer, "{\"rssi\": %d,\"heap\": %f, \"temp\": %f}", WiFi.RSSI(),getMemUsage(), temperatureRead());
            publish((char*)"diag");
          }
        }
      }
    }
  }
}