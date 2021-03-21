/*********************************************************************************
 * sensor.h - wrapper for HTU21D and BME280 libraries
 * 
 * User configurable via the following macros:
 *    #define BME280                  // if present uses BME280 library, otherwise HTU21D
 *    #define IMPERIAL                // if present returns readings in °F and inHg, otherwise °C and mb
 *    #define TEMP_OFFSET <float>     // if present, adds this value to temperature values
 *    #define HUMIDITY_OFFSET <float> // if present, adds this value to humidity values
 * 
 * -- Yuri - Mar 2021
*********************************************************************************/

#ifndef SENSOR_H
#define SENSOR_H

#include <Wire.h>

#ifdef BME280
#include <Adafruit_BME280.h>
#define SENSOR_TYPE Adafruit_BME280
#else
#include <SparkFunHTU21D.h>
#define SENSOR_TYPE HTU21D
#endif

#ifndef LIGHT_PIN
#define LIGHT_PIN A0
#endif

#ifndef ELEVATION
#define ELEVATION 0
#endif

#ifndef TEMP_OFFSET
#define TEMP_OFFSET 0
#endif

#ifndef HUMIDITY_OFFSET
#define HUMIDITY_OFFSET 0
#endif

#ifdef IMPERIAL
#define CONVERT_TEMP(t)  ( t * 1.8 + 32 + TEMP_OFFSET )     // °C to °F
#define CONVERT_PRESS(p) ( ((p / pow((1 - (ELEVATION / 3.2808) / 44330.0), 5.255))) / 3386.0 ) // abs Pa to rel inHg
#define TEMP_UNIT "F"
#define PRESS_UNIT "inHg"
#else
#define CONVERT_TEMP(t)  ( t + TEMP_OFFSET ) // °C
#define CONVERT_PRESS(p) ( (p / pow((1 - ELEVATION / 44330.0), 5.255)) / 100.0 )    // abs Pa to rel mb
#define TEMP_UNIT "C"
#define PRESS_UNIT "mb"
#endif

class Sensor
{
  public:
    Sensor() {}
    Sensor(int);
    void  begin()        { _sensor.begin(); }
    bool  poll();
    float light()        { return _light; }
    float temperature()  { return CONVERT_TEMP(_temp); }
    float humidity()     { return _humidity + HUMIDITY_OFFSET; }
#ifdef BME280
    float pressure()     { return CONVERT_PRESS(_pressure); }
#endif
  private:
    SENSOR_TYPE _sensor;
    int   _light_pin;
    float _light;
    float _temp;
    float _humidity;
#ifdef BME280
    float _pressure;
#endif
};

Sensor::Sensor(int light_pin) {
  _light_pin = light_pin;
}

bool Sensor::poll() {
  float l = analogRead(_light_pin) / 40.95; // percentage of ESP32 ADC resolution
  float t = _sensor.readTemperature();
  float h = _sensor.readHumidity();
#ifdef BME280
  float p =_sensor.readPressure();
#endif
  if ( t < 200 && h < 200 ) { // avoids occasional errors from HTU21D readings
    _light    = l;
    _temp     = t;
    _humidity = h;
#ifdef BME280
    _pressure = p;
#endif
    return true;
  }
  return false;
}

#endif // SENSOR_H
