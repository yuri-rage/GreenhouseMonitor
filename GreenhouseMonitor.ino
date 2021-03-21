/*********************************************************************************
 * Greenhouse Monitor
 * 
 * Web served temperature, humidity, and light level monitoring/logging
 * 
 * Hardware used: ESP32 Dev Board (see note below)
 *                SSD1306 128*64 OLED
 *                Sparkfun HTU21D Breakout or Adafruit BME280 Breakout
 *                Simple LDR photoresistor (with an LED dimming sticker applied)
 *                  The light value is neither calibrated nor linear,
 *                  but it seems reasonably useful for this case.
 *                
 *                Note: This sketch will not comppile for an ESP8266 or Arduino
 *                      variant.  It *should* be trivial to refactor for other
 *                      boards, but code modification would be necessary, since
 *                      this sketch relies on ESP32-centric functions and dependencies.
 *                
 * Dependencies: SparkFunHTU21D    - Arduino IDE Library Manager (if using HTU21D)
 *               Adafruit_Sensor   - Arduino IDE Library Manager (if using BME280)
 *               Adafruit_BME280   - Arduino IDE Library Manager (if using BME280)
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
//#define _DEBUG_MODE         // uncomment this line to include debug features
#define HOSTNAME "greenhouse" // <HOSTNAME>.local *should* work for mDNS browsing
#define TCP_PORT           80 // TCP port for ESPAsyncWebServer (80)
#define MAX_LOG_ENTRIES    49 // max number of entries in the datalog 
#define LOG_INTERVAL       15 // minutes between datalog entries
#define POLL_INTERVAL       5 // seconds between sensor readings
#define OLED_I2C         0x3C // display I2C address
#define LIGHT_PIN          A0 // analog pin for photoresistor (LDR)
#define IMPERIAL              // comment this line to use metric units
#define BME280                // comment this line if using HTU21D

// if using BME280 and you want to approximate local relative pressure,
//   enter local elevation above sea level
#define ELEVATION         586 // feet if using Imperial units, otherwise meters

// if you've calibrated your HTU21D or BME280 against known temp/humidity values,
//   uncomment these lines as needed to apply fixed correction(s) to the sensor return values
#define TEMP_OFFSET      -5.0
#define HUMIDITY_OFFSET   4.4

#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

#include "auth.h"        // edit auth.h with your own wifi and OpenWeatherMap info
#include "web_server.h"

SSD1306AsciiWire oled;
Web_Server server(TCP_PORT, LIGHT_PIN);

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

  Serial.print(F("  Initializing display..."));
  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x64, OLED_I2C);
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
  configTime(0, 0, "pool.ntp.org");              // set clock to UTC
  Serial.println(F("SET\n"));

  Serial.println(F("  Starting data services:"));
  server.begin();
  MDNS.addService("http", "tcp", TCP_PORT);

  Serial.printf("\n  Access via: %s / %s.local\n", WiFi.localIP().toString().c_str(), WiFi.getHostname());
  oled.clear();
  oled.printf("IP  : %s\nHost: %s", WiFi.localIP().toString().c_str(), WiFi.getHostname());

  delay(100);
  
  Serial.println(F("\n*****        Configuration Complete!       *****\n"));
}

unsigned long last_log_millis = 0;
unsigned long last_poll_millis = 0;

void loop(){
  unsigned long esp_millis = millis();

  if (esp_millis - last_poll_millis > POLL_INTERVAL * 1000 || last_poll_millis == 0) {

    if (server.data->poll()) {
      char buf[18];
      char valBuf[4];

      last_poll_millis = esp_millis;
      oled.setFont(CalLite24);
      oled.setCursor(0, 2);
      dtostrf(server.data->temperature(), 3, 0, valBuf);
      oled.print(valBuf);
      oled.setFont(lcd5x7);
      oled.setLetterSpacing(3);
      oled.print(F("o"));  // what a stupid way to print a "degree" symbol...but that's my workaround
                           //   if RAM and performance were a bigger concern, loading these fonts
                           //   so often (and using more than one) would be a terrible idea
      oled.setFont(CalLite24);
      strcpy(buf, TEMP_UNIT);
      strcat(buf, " ");
      dtostrf(server.data->humidity(), 3, 0, valBuf);
      strcat(buf, valBuf);
      strcat(buf, "% ");
      oled.print(buf);
      oled.setFont(Arial_bold_14);
      int iLight = round(server.data->light());
      sprintf(buf, "   Light: %d%%   ", iLight);
      size_t size = oled.strWidth(buf);
      oled.setCursor((oled.displayWidth() - size) / 2, 6);  // centered text
      oled.print(buf);
    } // else we just try to poll again on the next iteration
  }
  
  if (esp_millis - last_log_millis > LOG_INTERVAL * 60000 || last_log_millis == 0) {
    Serial.print(F("Logging data..."));
    server.data->log();
    last_log_millis = esp_millis;
    Serial.println(F("DONE"));
  }
}
