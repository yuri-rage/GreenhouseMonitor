/*********************************************************************************
 * datalogger.h - ArduinoJson v6 datalogging for GreenhouseMonitor.ino
 * 
 * -- Yuri - Mar 2021
*********************************************************************************/

#include <ArduinoJson.h>
#include <AsyncJson.h>

#ifndef MAX_LOG_ENTRIES
  #define MAX_LOG_ENTRIES 49  // max number of entries in the datalog
#endif
#ifndef LOG_INTERVAL
  #define LOG_INTERVAL    15  // minutes between datalog entries
#endif

int log_index = MAX_LOG_ENTRIES - 1;
DynamicJsonDocument doc(MAX_LOG_ENTRIES * 100); // *should* be enough space to hold datalog
                                                // log entries approaching 200 will exceed heap size
JsonObject docRoot;
JsonArray docArray;
DynamicJsonDocument sensorDoc(64);
DynamicJsonDocument owmDoc(64);

void initJsonDoc() {
  docRoot = doc.to<JsonObject>();
  docRoot["data_length"] = MAX_LOG_ENTRIES;
  docRoot["begin_index"] = 0;
  docArray = docRoot.createNestedArray("data");
  for (int i = 0; i < MAX_LOG_ENTRIES ; i++) {
    JsonObject entry = docArray.createNestedObject();
    entry["timestamp"] = 0;
    entry["temperature"] = 0;
    entry["humidity"] = 0;
    entry["light"] = 0;
  }
}

void jsonifySensorData(JsonObject &root) {
  root["temperature"] = temp;
  root["humidity"] = humidity;
  root["light"] = light;
#ifdef _DEBUG_MODE
  root["free_heap"] = ESP.getFreeHeap();
#endif
}

void jsonifyOWMInfo(JsonObject &root) {
  root["api_key"] = owm_key; // poor practice - exposes API key to the client
                             //   for this project, the risk is minor, and I'm accepting it
  root["city"] = owm_city;
}

// designed to be used after time is configured with via ntp
// on error returns processor uptime in seconds
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(millis() / 1000);
  }
  time(&now);
  return now;
}

void logData(float temp, float humd, float lt) {
  log_index = (log_index + 1) % MAX_LOG_ENTRIES;
  docArray[log_index]["timestamp"] = getTime();
  docArray[log_index]["temperature"] = temp;
  docArray[log_index]["humidity"] = humd;
  docArray[log_index]["light"] = lt;
  docRoot["begin_index"] = (log_index + 1) % MAX_LOG_ENTRIES;  // this is the oldest entry
  // serializeJsonPretty(docRoot, Serial);Serial.println();
}
