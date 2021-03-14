/*********************************************************************************
 * auth.h - WiFI and OpenWeatherMap info for GreenhouseMonitor.ino
 * 
 * Enter your WiFi SSID and password below
 * 
 * A free OpenWeatherMap API key can be obtained at https://openweathermap.org/
 * 
 * The owm_city string can be formatted: "city,country" or "city,state,US"
 *     OWM uses ISO1366 two letter country codes
 *     You can search for your city and country code at https://openweathermap.org/
 * 
 * -- Yuri - Mar 2021
*********************************************************************************/

const char* ssid     = "SSID";
const char* password = "password";
const char* owm_key  = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char* owm_city = "Somerset,MA,US";  // or like "Ivrea/IT"
