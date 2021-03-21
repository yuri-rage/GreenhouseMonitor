/*********************************************************************************
 * datalogger.h - ArduinoJson v6 datalogging for GreenhouseMonitor.ino
 *              - extends Sensor class for direct access to sensor values
 * 
 * -- Yuri - Mar 2021
*********************************************************************************/

#ifndef DATA_H
#define DATA_H

#include <ArduinoJson.h>
#include "auth.h"
#include "sensor.h"

#ifndef MAX_LOG_ENTRIES
  #define MAX_LOG_ENTRIES 49  // max number of entries in the datalog
#endif
#ifndef LOG_INTERVAL
  #define LOG_INTERVAL    15  // minutes between datalog entries
#endif

class Data : public Sensor {
  public:
    Data();
    Data(int);
    void begin();
    void log();
    void jsonifyLog(String&);
    void jsonifySensorData(String&);
    void jsonifyConfigData(String&);
#ifdef BME280
    // adding pressure to the datalog tends to bog AJAX requests, so store it separately
    void jsonifyPLog(String&);
  private:
    DynamicJsonDocument _docPLog;
    JsonObject _docPRoot;
    JsonArray _docPArray;
#else
  private:
#endif
    int _index = MAX_LOG_ENTRIES - 1;
    DynamicJsonDocument _docLog;
    JsonObject _docLogRoot;
    JsonArray _docLogArray;
    DynamicJsonDocument _sensorDoc;
    DynamicJsonDocument _configDoc;
    unsigned long _getTime();
};


Data::Data():Sensor(), _docLog(0),
                       _sensorDoc(0),
                       _configDoc(0)
#ifdef BME280
                     , _docPLog(0)
#endif
                       {}

Data::Data(int light_pin):Sensor(light_pin), _docLog(MAX_LOG_ENTRIES * 100),  // *should* hold up to MAX_LOG_ENTRIES≈200
                                             _sensorDoc(128),
                                             _configDoc(128)
#ifdef BME280
                                           , _docPLog(MAX_LOG_ENTRIES * 50)
#endif
                                             {}

void Data::begin() {
  _docLogRoot = _docLog.to<JsonObject>();
  _docLogRoot["data_length"] = MAX_LOG_ENTRIES;
  _docLogRoot["begin_index"] = 0;
  _docLogArray = _docLogRoot.createNestedArray("data");
#ifdef BME280
  _docPRoot = _docPLog.to<JsonObject>();
  _docPRoot["data_length"] = MAX_LOG_ENTRIES;
  _docPRoot["begin_index"] = 0;
  _docPArray = _docPRoot.createNestedArray("data");
#endif
  for (int i = 0; i < MAX_LOG_ENTRIES ; i++) {
    JsonObject entry = _docLogArray.createNestedObject();
    entry["timestamp"] = 0;
    entry["temperature"] = 0;
    entry["humidity"] = 0;
    entry["light"] = 0;
#ifdef BME280
    JsonObject entryP = _docPArray.createNestedObject();
    entryP["timestamp"] = 0;
    entryP["pressure"] = 0;
#endif
  }
  Sensor::begin();
}

void Data::log() {
  _index = (_index + 1) % MAX_LOG_ENTRIES;
  _docLogArray[_index]["timestamp"]   = this->_getTime();
  _docLogArray[_index]["temperature"] = this->temperature();
  _docLogArray[_index]["humidity"]    = this->humidity();
  _docLogArray[_index]["light"]       = this->light();
  _docLogRoot["begin_index"] = (_index + 1) % MAX_LOG_ENTRIES;  // this is the oldest entry
#ifdef BME280
  _docPArray[_index]["timestamp"]   = this->_getTime();
  _docPArray[_index]["pressure"]      = this->pressure();
  _docPRoot["begin_index"] = (_index + 1) % MAX_LOG_ENTRIES;  // this is the oldest entry
#endif
}

void Data::jsonifyLog(String &s) {
  serializeJson(_docLogRoot, s);
}

#ifdef BME280
void Data::jsonifyPLog(String &s) {
  serializeJson(_docPRoot, s);
}
#endif

void Data::jsonifySensorData(String &s) {
  JsonObject root = _sensorDoc.to<JsonObject>();
  root["temperature"] = this->temperature();
  root["humidity"] = this->humidity();
  root["light"] = this->light();
#ifdef BME280
  root["pressure"] = this->pressure();
#endif
#ifdef _DEBUG_MODE
  root["free_heap"] = ESP.getFreeHeap();
#endif
  serializeJson(root, s);
}

void Data::jsonifyConfigData(String &s) {
  JsonObject root = _configDoc.to<JsonObject>();
  root["api_key"] = owm_key; // poor practice - exposes API key to the client
                             //   for this project, the risk is minor, and I'm accepting it
  root["city"] = owm_city;
  root["temp_unit"] = TEMP_UNIT;
#ifdef BME280
  root["press_unit"] = PRESS_UNIT;
#endif
  serializeJson(root, s);
}

// designed to be used after time is configured with via ntp
// on error returns processor uptime in seconds
unsigned long Data::_getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(millis() / 1000);
  }
  time(&now);
  return now;
}

#endif // DATA_H
