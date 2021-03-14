/*********************************************************************************
 * Greenhouse Monitor
 * 
 * Web served temperature, humidity, and light level monitoring/logging
 * 
 * Hardware used: ESP32 Dev Board (ESP32-specific libraries are used)
 *                Sparkfun HTU21D Breakout
 *                Simple LDR photoresistor (with an LED dimming sticker applied)
 *                  The light value is neither calibrated nor linear,
 *                  but it seems reasonably useful for this case.
 *                SSD1306 128*64 OLED
 *                
 * Dependencies: SparkFunHTU21D    - Arduino IDE Library Manager
 *               SSD1306Ascii      - Arduino IDE Library Manager
 *               ArduinoJson       - Arduino IDE Library Manager
 *               ESPAsyncWebServer - https://github.com/me-no-dev/ESPAsyncWebServer
 *
 * -- Yuri - Mar 2021
 * 
 * Based on code by Rui Santos - https://RandomNerdTutorials.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files.
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
*********************************************************************************/

/*** change these as desired ***
  max log entries of 49 seems reasonable - setting >200 will likely overrun ESP32 memory
    for a 12 hour datalog, use a log interval of 15 mins with 49 entries (48 + 1 to account for total time elapsed)
    30 mins for 24 hrs, 60 mins for 48 hrs
  poll intervals faster than a few seconds may slightly heat the sensor and cause erroneous readings */
#define HOSTNAME "greenhouse" // <HOSTNAME>.local *should* work for mDNS browsing
#define MAX_LOG_ENTRIES    49 // max number of entries in the datalog 
#define LOG_INTERVAL       15 // minutes between datalog entries
#define POLL_INTERVAL       5 // seconds between sensor readings
#define OLED_I2C         0x3C // display I2C address
#define _DEBUG_MODE         // comment this line to compile without extra debug features

#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <SparkFunHTU21D.h>  // verified against a more complex library that the HTU21D
                             //   heater is defaulted to off in this library
                             //   still, the temp value seems a 1-3° high,
                             //   and the humidity seems a tad low
                             //   I may take some measurements and simply hard code some offsets
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// globals - maybe I'll refactor to make this less ugly (they should likely be in a class object)
//           for now, they are referenced by the locally imported files below
float temp;
float humidity;
float light;
const int ldrPin = A0;
HTU21D htu21;
SSD1306AsciiWire oled;
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 0; // -21600;  // America/CST
const int   daylightOffset_sec = 0; // 3600;    // use DST

#include "auth.h"       // edit auth.h with your own wifi and OpenWeatherMap info
#include "webserver.h"

boolean connectWiFi(int timeout) {
  unsigned long startMillis = millis();
  WiFi.disconnect(true);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(ssid, password);
  WiFi.setHostname(HOSTNAME);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    oled.print(F("."));
    if (millis() - startMillis > timeout) {
      return false;
    }
  }
  return true;
}

void setup(){
  Serial.begin(115200);

  Serial.println(F("\n\n*****    Configuring Greenhouse Monitor    *****\n"));

  Serial.print(F("  Initializing sensors and datalogger..."));
  htu21.begin();
  oled.begin(&Adafruit128x64, OLED_I2C);
  initJsonDoc();
  Serial.println(F("DONE\n"));

  oled.setFont(lcd5x7);
  oled.clear();
  oled.print(F("Initializing..."));
  
  Serial.print(F("  Preparing SPIFFS..."));
  if(!SPIFFS.begin()){
    Serial.println(F("ERROR: SPIFFS not mounted"));
    return;
  }
  Serial.println(F("READY\n"));

  Serial.printf("  Connecting to %s..", ssid);

  int wifiAttempts = 0;
  while (!connectWiFi(5000)) {
    wifiAttempts++;
    if (wifiAttempts < 3) {
      Serial.printf("RESET %d.", wifiAttempts);
    } else {
      Serial.println(F("FAILED\n\nRebooting!"));
      delay(1000);
      ESP.restart();
    }
  }

  Serial.println(F("CONNECTED\n"));

  Serial.print(F("  Starting mDNS service..."));
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println(F("FAILED\n"));
  }
  Serial.println(F("STARTED\n"));

  Serial.print(F("  Setting time from NTP..."));
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println(F("SET\n"));

  Serial.print(F("  Starting web server..."));
  initWebServer();
  MDNS.addService("http", "tcp", 80);

  Serial.printf("ACTIVE ON %s / %s.local\n", WiFi.localIP().toString().c_str(), WiFi.getHostname());
  oled.clear();
  oled.printf("IP  : %s\nHost: %s", WiFi.localIP().toString().c_str(), WiFi.getHostname());

  delay(100);
  
  Serial.println(F("\n*****        Configuration Complete!       *****"));
  Serial.println(F("\nMonitoring..."));
}

unsigned long last_log_millis = 0;
unsigned long last_poll_millis = 0;

void loop(){
  unsigned long esp_millis = millis();

  if (esp_millis - last_poll_millis > POLL_INTERVAL * 1000 || last_poll_millis == 0) {
    float t = htu21.readTemperature();
    float h = htu21.readHumidity();
    light = analogRead(ldrPin) / 40.95;

    // occasionally htu21 reports erroneously high values (1828°F with 998% humidity)
    if (temp < 200.0 && humidity < 200.0) {
      char buf[18];
      char valBuf[4];
      humidity = h + (25.0 - t) * -0.15;  // compensation values from HTU21D datasheet
      temp = t * 1.8 + 32.0;              // to Fahrenheit because 'Murica I suppose...
      last_poll_millis = esp_millis;
      oled.setFont(CalLite24);
      oled.setCursor(0, 2);
      dtostrf(temp, 3, 0, valBuf);
      oled.print(valBuf);
      oled.setFont(lcd5x7);
      oled.setLetterSpacing(3);
      oled.print(F("o"));  // what a stupid way to print a "degree" symbol...but that's my workaround
                           //   if RAM and performance were a bigger concern, loading these fonts
                           //   so often (and using more than one) would be a terrible idea
      oled.setFont(CalLite24);
      strcpy(buf, "F ");
      dtostrf(humidity, 3, 0, valBuf);
      strcat(buf, valBuf);
      strcat(buf, "% ");
      oled.print(buf);
      oled.setFont(Arial_bold_14);
      int iLight = round(light);
      sprintf(buf, "   Light: %d%%   ", iLight);
      size_t size = oled.strWidth(buf);
      oled.setCursor((oled.displayWidth() - size) / 2, 6);  // centered text
      oled.print(buf);
    } // else we just try to poll again on the next iteration
  }
  
  if (esp_millis - last_log_millis > LOG_INTERVAL * 60000 || last_log_millis == 0) {
    Serial.print(F("Logging data..."));
    logData(temp, humidity, light);
    last_log_millis = esp_millis;
    Serial.println(F("DONE"));
  }
}
