const char *header = 
"HTTP/1.1 200 OK\n\
Content-type:text/html\n\
Connection: close\n\n";

const char *noContent = 
"HTTP/1.1 204 OK\n\
Content-type:text/html\n\
Connection: close\n\n";

const char *root =
"<html><body>\
<form action=\"/submit\" method=\"post\">\
<table><tbody><tr>\
  <td><label for=\"ssid\">SSID</label></td>\
  <td><input type=\"text\" name=\"ssid\" id=\"ssid\"></td></tr>\
  <td><label for=\"password\">Password</label></td>\
  <td><input type=\"text\" name=\"password\" id=\"password\"></td></tr>\
  <td><label for=\"server\">MQTT server address</label></td>\
  <td><input type=\"text\" name=\"server\" id=\"server\"></td></tr>\
  <td><label for=\"topic\">MQTT topic</label></td>\
  <td><input type=\"text\" name=\"topic\" id=\"topic\"></td></tr>\
  <td><label for=\"mqttuser\">MQTT username</label></td>\
  <td><input type=\"text\" name=\"mqttuser\" id=\"mqttuser\"></td></tr>\
  <td><label for=\"mqttpass\">MQTT password</label></td>\
  <td><input type=\"text\" name=\"mqttpass\" id=\"mqttpass\"></td></tr>\
  <td><label for=\"interval\">Data interval</label></td>\
  <td><input type=\"number\" name=\"interval\" id=\"interval\"></td></tr>\
</tbody></table>\
<input type=\"submit\" value=\"Submit\">\
</form>\
</body></html>";