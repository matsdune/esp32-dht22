const char *header = 
"HTTP/1.1 200 OK\n\
Content-type:text/html\n\
Connection: close\n\n";

const char *noContent = 
"HTTP/1.1 204 OK\n\
Content-type:text/html\n\
Connection: close\n\n";

const char *head = "<html><head><style>html{font-family:sans-serif;line-height:1.15;-webkit-text-size-adjust:100%%;-webkit-tap-highlight-color:#000}body{margin:0;font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,\"Helvetica Neue\",Arial,\"Noto Sans\",sans-serif;font-size:1rem;color:#212529;text-align:left;background-color:#fff}h3{margin-top:0;margin-bottom:.5rem;font-weight:500;font-size:1.75rem}label{display:inline-block;margin-bottom:.5rem}input{margin:0;font-family:inherit;font-size:inherit;overflow:visible}[type=submit]{-webkit-appearance:button;cursor:pointer}[type=submit]::-moz-focus-inner{padding:0;border-style:none}[type=number]::-webkit-inner-spin-button,[type=number]::-webkit-outer-spin-button{height:auto}.form-control{width:100%%;height:calc(1.5em + .75rem + 2px);padding:.375rem .75rem;border:1px solid #ced4da}.form-control::-ms-expand{background-color:transparent;border:0}.form-control:-moz-focusring{color:transparent;text-shadow:0 0 0 #495057}.form-control:focus{border-color:#80bdff;outline:0;box-shadow:0 0 0 .2rem rgba(0,123,255,.25)}.form-group{margin-bottom:1rem}.card{border:1px solid #dfdfdf}.card-body{padding:1.25rem;border-bottom:1px solid #dfdfdf}.card-footer,.card-header{padding:.75rem 1.25rem;background-color:#f7f7f7;border-bottom:1px solid #dfdfdf}</style></head>";

const char *root = "<body><div class=\"card\"><div class=\"card-header\"><h3>Mqtt sensor config</h3></div><form action=\"/submit\" method=\"post\"><div class=\"card-body\"><div class=\"form-group\"><label for=\"ssid\">SSID</label><input class=\"form-control\" type=\"text\" name=\"ssid\" id=\"ssid\"></div><div class=\"form-group\"><label for=\"password\">Password</label><input class=\"form-control\" type=\"text\" name=\"password\" id=\"password\"></div><div class=\"form-group\"><label for=\"server\">MQTT server address</label><input class=\"form-control\" type=\"text\" name=\"server\" id=\"server\"></div><div class=\"form-group\"><label for=\"topic\">MQTT topic</label><input class=\"form-control\" type=\"text\" name=\"topic\" id=\"topic\"></div><div class=\"form-group\"><label for=\"mqttuser\">MQTT username</label><input class=\"form-control\" type=\"text\" name=\"mqttuser\" id=\"mqttuser\"></div><div class=\"form-group\"><label for=\"mqttpass\">MQTT password</label><input class=\"form-control\" type=\"text\" name=\"mqttpass\" id=\"mqttpass\"></div><div class=\"form-group\"><label for=\"interval\">Data interval</label><input class=\"form-control\" type=\"number\" name=\"interval\" id=\"interval\"></div><div class=\"form-group\"><label for=\"sensors\">Number of sensors</label><input class=\"form-control\" type=\"number\" name=\"sensors\" id=\"sensors\" value=\"3\" min=\"1\" max=\"3\"></div></div><div class=\"card-footer\"><input type=\"submit\" value=\"Submit\"><div style=\"float: right;\"><a href=\"/signal\">Signal</a></div></div></form></div></body></html>";

void renderPage(WiFiClient* client, const char *page = ""){
  if(strcmp(page, "noContent") == 0 ){
    client->printf(noContent);
  }
  else{
    client->printf(header);
    if(strcmp(page, "root") == 0){
      client->printf(head);
      client->printf(root);
    }
  }

}