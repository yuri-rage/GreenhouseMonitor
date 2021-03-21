/*********************************************************************************
 * web_server.h - extends ESPAsyncWebServer for GreenhouseMonitor.ino
 * 
 * -- Yuri - Mar 2021
*********************************************************************************/

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include "data.h"

Data* _data; // needs to be accessible to lambda/static functions
             // I hate this global, but my C++ skills failed here

class Web_Server : public AsyncWebServer{
  public:
    Web_Server(int port, int light_pin);
    void begin();
    Data* data;  // access global Data object as if it's a class member
};

Web_Server::Web_Server(int port, int light_pin):AsyncWebServer(port) {
  _data = new Data(light_pin);
  data = _data;
}

void Web_Server::begin() {

  Serial.print(F("    Adding web server routes..."));

  this->onNotFound([](AsyncWebServerRequest *request){
    Serial.print(F("Bad request, sending 404..."));
    request->send(404, "text/plain", F("404...that's a hard no, super chief."));
    Serial.println(F("SENT"));
  });
  this->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Index page requested..."));
    request->send(SPIFFS, "/index.html");
    Serial.println(F("SENT"));
  });
  this->on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Favicon requested..."));
    request->send(SPIFFS, "/favicon.svg");
    Serial.println(F("SENT"));
  });
  this->on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Temperature requested..."));
    char buf[7];
    dtostrf(_data->temperature(), 3, 1, buf);
    request->send_P(200, "text/plain", buf);
    Serial.println(_data->temperature());
  });
  this->on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Humidity requested..."));
    char buf[7];
    dtostrf(_data->humidity(), 3, 1, buf);
    request->send_P(200, "text/plain", buf);
    Serial.println(_data->humidity());
  });
  this->on("/light", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Light requested..."));
    char buf[7];
    dtostrf(_data->light(), 3, 1, buf);
    request->send_P(200, "text/plain", buf);
    Serial.println(_data->light());
  });
#ifdef BME280
  this->on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.print(F("Pressure requested..."));
    char buf[7];
    dtostrf(_data->pressure(), 3, 1, buf);
    request->send_P(200, "text/plain", buf);
    Serial.println(_data->pressure());
  });
#endif

  this->on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("Sensor data requested..."));
    String response;
    _data->jsonifySensorData(response);
    request->send(200, "application/json", response);
    Serial.println(response);
  });

  this->on("/configdata", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("Config data requested..."));
    String response;
    _data->jsonifyConfigData(response);
    request->send(200, "application/json", response);
    Serial.println(response);
  });
  
  this->on("/datalog", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("Datalog requested..."));
    String response;
    _data->jsonifyLog(response);
    request->send(200, "application/json", response);
    Serial.println(F("SENT"));
  });

#ifdef BME280
  this->on("/pressurelog", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("Pressure log requested..."));
    String response;
    _data->jsonifyPLog(response);
    request->send(200, "application/json", response);
    Serial.println(F("SENT"));
  });
#endif

#ifdef _DEBUG_MODE
  // this allows requests from locally stored documents on the client device
  //   (i.e., C:\users\yuri\Documents\debug.html, /home/yuri/Documents/test.html, etc)
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
#endif

  Serial.println(F("DONE"));
  Serial.print(F("    Initializing sensors and datalog..."));
  data->begin();
  Serial.println(F("DONE"));
  Serial.print(F("    Starting web server..."));
  Serial.println(F("READY"));
  AsyncWebServer::begin();
}

#endif // WEB_SERVER_H
