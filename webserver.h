/*********************************************************************************
 * webserver.h - ESPAsyncWebServer initialization for GreenhouseMonitor.ino
 * 
 * -- Yuri - Mar 2021
*********************************************************************************/

#include <ESPAsyncWebServer.h>
#include "datalogger.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

void initWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Index page requested..."));
    request->send(SPIFFS, "/index.html");
    Serial.println(F("SENT"));
  });
  server.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Favicon requested..."));
    request->send(SPIFFS, "/favicon.svg");
    Serial.println(F("SENT"));
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Temperature requested..."));
    char buf[7];
    dtostrf(temp, 3, 1, buf);
    request->send_P(200, "text/plain", buf);
    Serial.println(temp);
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Humidity requested..."));
    char buf[7];
    dtostrf(humidity, 3, 1, buf);
    request->send_P(200, "text/plain", buf);
    Serial.println(humidity);
  });
  server.on("/light", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Light requested..."));
    char buf[7];
    dtostrf(light, 3, 1, buf);
    request->send_P(200, "text/plain", buf);
    Serial.println(light);
  });

  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("Sensor data requested..."));
    String response;
    JsonObject sensorRoot = sensorDoc.to<JsonObject>();
    jsonifySensorData(sensorRoot);
    serializeJson(sensorRoot, response);
    request->send(200, "application/json", response);
    serializeJson(sensorRoot, Serial);
    Serial.println();
  });

  server.on("/owm", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("OWM info requested..."));
    String response;
    JsonObject owmRoot = owmDoc.to<JsonObject>();
    jsonifyOWMInfo(owmRoot);
    serializeJson(owmRoot, response);
    request->send(200, "application/json", response);
    serializeJson(owmRoot, Serial);
    Serial.println();
  });
  
  server.on("/datalog", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("Datalog requested..."));
    String response;
    serializeJson(docRoot, response);
    request->send(200, "application/json", response);
    Serial.println(F("SENT"));
  });

#ifdef DEBUG_MODE
  // this allows requests from locally stored documents on the client device
  //   (i.e., C:\users\yuri\Documents\debug.html, /home/yuri/Documents/test.html, etc)
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
#endif
  server.begin();
}
