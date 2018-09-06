/**The MIT License (MIT)

Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at https://thingpulse.com
*/

// Depends on the following libraries:
// - Time library: https://github.com/PaulStoffregen/Time
// - DHT Sensor Library for ESP32: https://github.com/beegee-tokyo/DHTesp
// - SSD1306 display library: https://github.com/ThingPulse/esp8266-oled-ssd1306
// - esp8266-weather-station: https://github.com/gandalf15/esp8266-weather-station

#include <DHTesp.h>
#include <Ticker.h>
#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h>
#include <time.h>     // time() ctime()
#include <sys/time.h> // struct timeval

#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "images.h"

#ifndef ESP32
#pragma message(THIS IS FOR ESP32 ONLY !)
#error Select ESP32 board.
#endif

/*******************************************
 * Begin Settings
 *******************************************/
// DHT Temperature / Humidity Settings

#define DHTPIN 16 // Pin which is connected to the DHT sensor.
// The interval is read from the sensor's data in setup function
const uint32_t updateDHTIntervalSec = 10; // Must be at least 2 sec

// WIFI Settings
const char *WIFI_SSID = "YourWiFiNameHere";
const char *WIFI_PWD = "YourWiFiPasswordHere";

// Time zone Settings
#define TZ 0      // (utc+) TZ in hours
#define DST_MN 60 // use 60min for summer time in some countries

// Weather interval settings
const int UPDATE_WEATHER_INTERVAL_SECS = 10 * 60; // Update every 10 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = 5; //pin on ESP32
const int SDC_PIN = 4; // pin on ESP32

// OpenWeatherMap Settings
// Sign up here to get an API key:
// https://docs.thingpulse.com/how-tos/openweathermap-key/
const boolean IS_METRIC = true; // true = *C, false = *F
const String OPEN_WEATHER_MAP_APP_ID = "Your_API_ID_here";
// find your city ID here: http://bulk.openweathermap.org/sample/city.list.json.gz
const String OPEN_WEATHER_MAP_LOCATION = "Your_city_ID_here"; // 3333224 = Aberdeen,GB

// Pick a language code from this list:
// Arabic - ar, Bulgarian - bg, Catalan - ca, Czech - cz, German - de, Greek - el,
// English - en, Persian (Farsi) - fa, Finnish - fi, French - fr, Galician - gl,
// Croatian - hr, Hungarian - hu, Italian - it, Japanese - ja, Korean - kr,
// Latvian - la, Lithuanian - lt, Macedonian - mk, Dutch - nl, Polish - pl,
// Portuguese - pt, Romanian - ro, Russian - ru, Swedish - se, Slovak - sk,
// Slovenian - sl, Spanish - es, Turkish - tr, Ukrainian - ua, Vietnamese - vi,
// Chinese Simplified - zh_cn, Chinese Traditional - zh_tw.

const String OPEN_WEATHER_MAP_LANGUAGE = "en";
// this represents forecasts for every 3 hours in 5 days. (free subscriptions)
const uint8_t MAX_FORECASTS = 40;

// Adjust according to your language
const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
/*******************************************
 * End Settings
 *******************************************/

/*******************************************
 * Start global variables
 *******************************************/
// Initialize the oled display
SSD1306Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi ui(&display);

OpenWeatherMapCurrentData currentWeatherData;
OpenWeatherMapCurrent currentWeatherClient;

OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

#define TZ_MN ((TZ)*60)
#define TZ_SEC ((TZ)*3600)
#define DST_SEC ((DST_MN)*60)
time_t now;

// flag
bool readyForWeatherUpdate = false;

long timeSinceLastWUpdate = 0;

// DHT sensor
DHTesp dht;
// Data from DHT sensor
TempAndHumidity dhtSensorData;
// Task handle for the value read task
TaskHandle_t dhtTaskHandle = NULL;
// Ticker for temperature and humidity reading
Ticker dhtTicker;
// Comfort profile
ComfortState comfState;
// Flag if task should run
bool dhtTaskEnabled = false;
/*******************************************
 * End global variables
 *******************************************/

//Function prototypes
void dhtTask(void *pvParameters);
void triggerGetTempAndHumidity();

void updateWeatherData(OLEDDisplay *display);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawPictures(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawInsideTemp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawInsideHum(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawCurrentWindAndHum(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawTodaysForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawFooter(OLEDDisplay *display, OLEDDisplayUiState *state, int x, int y);
// void drawNextDaysForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
// void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
// void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);

// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = {drawDateTime, drawPictures, drawInsideTemp, drawInsideHum,
                          drawCurrentWeather, drawCurrentWindAndHum, drawForecast};
int numberOfFrames = 7;

// OverlayCallback overlays[] = {drawHeaderOverlay};
// int numberOfOverlays = 1;

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // Initialize DHT sensor.
  dht.setup(DHTPIN, DHTesp::DHT22);
  Serial.println("DHT initialised");

  // Start task to read DHT sensor
  xTaskCreatePinnedToCore(
      dhtTask,        /* Function to implement the task */
      "dhtTask ",     /* Name of the task */
      4000,           /* Stack size in words */
      NULL,           /* Task input parameter */
      5,              /* Priority of the task */
      &dhtTaskHandle, /* Task handle. */
      1);             /* Core where the task should run */

  if (dhtTaskHandle == NULL)
  {
    Serial.println("Failed to start task for DHT update");
  }
  else
  {
    // Start update of environment data every updateDHTIntervalSec seconds
    dhtTicker.attach(updateDHTIntervalSec, triggerGetTempAndHumidity);
  }

  // initialize dispaly
  display.init();
  display.clear();
  display.display();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  // Start WiFi
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    ++counter;
  }
  // Get time from network time service
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  // Setup user interface
  ui.setTargetFPS(30);

  // Set the symbol to indicate an active frame in the indicator bar.
  ui.setActiveSymbol(activeSymbole);
  // Set the symbol to indicate an inactive frame in the indicator bar.
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  // ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  // ui.setIndicatorDirection(LEFT_RIGHT);

  // Disable drawing of all indicators
  ui.disableAllIndicators();

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frame drawing functions
  ui.setFrames(frames, numberOfFrames);

  // Add overlays drawing functions that are draw independent of the Frames
  // ui.setOverlays(overlays, numberOfOverlays);

  // Set the approx. time a frame is displayed
  ui.setTimePerFrame(3000);

  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("Setup Finished");

  updateWeatherData(&display);
  // Signal end of setup() to dhtTask
  dhtTaskEnabled = true;
}

void loop()
{

  if (millis() - timeSinceLastWUpdate > (1000L * UPDATE_WEATHER_INTERVAL_SECS))
  {
    readyForWeatherUpdate = true;
    timeSinceLastWUpdate = millis();
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED)
  {
    updateWeatherData(&display);
  }

  // the remaining time (in ms)
  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0)
  {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.

    // Switch without transition to frame `frame`.
    // void switchToFrame(uint8_t frame);

    /**
    * Transition to frame `frame`, when the `frame` number is bigger than the current
    * frame the forward animation will be used, otherwise the backwards animation is used.
    */
    // void transitionToFrame(uint8_t frame);

    delay(remainingTimeBudget * 0.9);
  }
}

/**
 * Task to reads temperature from DHT22 sensor
 * @param pvParameters
 *    pointer to task parameters
 */
void dhtTask(void *pvParameters)
{
  Serial.println("dhtTask loop started");
  while (1)
  {
    if (dhtTaskEnabled)
    {
      dhtSensorData = dht.getTempAndHumidity();
      // Check if any reads failed
      if (dht.getStatus() != 0)
      {
        Serial.println("DHT22 error status: " + String(dht.getStatusString()));
      }
    }
    // Got sleep again
    vTaskSuspend(NULL);
  }
}

/**
 * triggerGetTempAndHumidity
 * resumes the dhtTask via dhtTaskHandle
 * called by Ticker dhtTicker
 */
void triggerGetTempAndHumidity()
{
  if (dhtTaskHandle != NULL)
  {
    xTaskResumeFromISR(dhtTaskHandle);
  }
}

void updateWeatherData(OLEDDisplay *display)
{
  drawProgress(display, 10, "Updating time");
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  drawProgress(display, 30, "Updating current weather");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeatherData, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION);

  drawProgress(display, 50, "Updating forecasts");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {00, 03, 6, 9, 12, 15, 18, 21};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION, MAX_FORECASTS);

  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done");
  delay(1000);
}

void drawProgress(OLEDDisplay *display, int percentage, String label)
{
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  now = time(nullptr);
  struct tm *timeInfo;
  timeInfo = localtime(&now);
  char buff[16];

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  String date = WDAY_NAMES[timeInfo->tm_wday];
  sprintf_P(buff, PSTR("%s %02d/%02d/%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year + 1900);
  display->drawString(64 + x, 0 + y, String(buff));

  display->setFont(ArialMT_Plain_24);
  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 18 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  drawFooter(display, state, x, y);
}

void drawPictures(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  now = time(nullptr);
  struct tm *timeInfo;
  timeInfo = localtime(&now);
  static int last_min;
  static int rand_num;
  if (last_min != timeInfo->tm_min)
  {
    last_min = timeInfo->tm_min;
    rand_num = rand();
  }
  int index = rand_num % (sizeof(images) / sizeof(Image) - 1);
  display->drawXbm((64 - images[index].width / 2) + x, (32 - images[index].height / 2) + y,
                   images[index].width, images[index].height, images[index].bits);
}

void drawInsideTemp(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  // Temperature
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 0 + y, "In");
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 18 + y, "Temp");
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(128 + x, 18 + y, "Feel");
  String temp;
  float heatIndex;
  if (IS_METRIC)
  {
    temp = String(dhtSensorData.temperature, 1) + "°C";
    heatIndex = dht.computeHeatIndex(dhtSensorData.temperature, dhtSensorData.humidity, false);
  }
  else
  {
    temp = String(dht.toFahrenheit(dhtSensorData.temperature), 1) + "°F";
    heatIndex = dht.computeHeatIndex(dht.toFahrenheit(dhtSensorData.temperature), dhtSensorData.humidity, true);
  }
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 34 + y, temp);
  // Heat index
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(128 + x, 34 + y, String(heatIndex, 1));
}

void drawInsideHum(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  // Humidity and comfort zone
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 0 + y, "In");
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 18 + y, "Humid");
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(128 + x, 18 + y, "Comfort");

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String humidity = String(dhtSensorData.humidity, 1) + "%";
  display->drawString(0 + x, 34 + y, humidity);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  float cr = dht.getComfortRatio(comfState, dhtSensorData.temperature, dhtSensorData.humidity);

  String comfortStatus;
  switch (comfState)
  {
  case Comfort_OK:
    display->setFont(ArialMT_Plain_16);
    comfortStatus = "OK";
    break;
  case Comfort_TooHot:
    display->setFont(ArialMT_Plain_16);
    comfortStatus = "Too Hot";
    break;
  case Comfort_TooCold:
    comfortStatus = "Too Cold";
    break;
  case Comfort_TooDry:
    display->setFont(ArialMT_Plain_16);
    comfortStatus = "Too Dry";
    break;
  case Comfort_TooHumid:
    comfortStatus = "Too Humid";
    break;
  case Comfort_HotAndHumid:
    comfortStatus = "Hot & Humid";
    break;
  case Comfort_HotAndDry:
    comfortStatus = "Hot & Dry";
    break;
  case Comfort_ColdAndHumid:
    comfortStatus = "Cold & Humid";
    break;
  case Comfort_ColdAndDry:
    comfortStatus = "Cold & Dry";
    break;
  default:
    comfortStatus = "Unknown";
    break;
  };
  display->drawString(128 + x, 40 + y, comfortStatus);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 0 + y, "Out");
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeatherData.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128 + x, 14 + y, temp);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(10 + x, 0 + y, currentWeatherData.iconMeteoCon);

  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(128 + x, 35 + y, currentWeatherData.description);

  now = time(nullptr);
  struct tm *timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 54 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String inTemp;
  if (IS_METRIC)
  {
    inTemp = String(dhtSensorData.temperature, 1) + "°C";
  }
  else
  {
    inTemp = String(dht.toFahrenheit(dhtSensorData.temperature), 1) + "°F";
  }
  inTemp = "IN: " + inTemp;
  display->drawString(128 + x, 54 + y, inTemp);
  display->drawHorizontalLine(0 + x, 52 + y, 128 + x);
}

void drawCurrentWindAndHum(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  // Humidity and comfort zone
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 0 + y, "Out");
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 18 + y, "Humid");
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(128 + x, 18 + y, "Wind");

  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String humidity = String(currentWeatherData.humidity) + " %";
  display->drawString(0 + x, 34 + y, humidity);
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String windSpeed = String(currentWeatherData.windSpeed, 1) + (IS_METRIC ? " m/s" : " mph");
  display->drawString(128 + x, 34 + y, windSpeed);
  drawFooter(display, state, x, y);
}

void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  drawTodaysForecastDetails(display, x, y, 1);
  drawTodaysForecastDetails(display, x + 44, y, 2);
  drawTodaysForecastDetails(display, x + 88, y, 3);
  drawFooter(display, state, x, y);
}

void drawTodaysForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex)
{
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm *timeInfo;
  timeInfo = localtime(&observationTimestamp);

  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, buff);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawFooter(OLEDDisplay *display, OLEDDisplayUiState *state, int x, int y)
{
  now = time(nullptr);
  struct tm *timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 54 + y, String(buff));

  String inTemp;
  if (IS_METRIC)
  {
    inTemp = String(dhtSensorData.temperature, 1) + "°C";
  }
  else
  {
    inTemp = String(dht.toFahrenheit(dhtSensorData.temperature), 1) + "°F";
  }
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  inTemp = "I:" + inTemp;
  display->drawString(38 + x, 54 + y, inTemp);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String outTemp = String(currentWeatherData.temp, 1) + (IS_METRIC ? "°C" : "°F");
  outTemp = "O:" + outTemp;
  display->drawString(128 + x, 54 + y, outTemp);
  display->drawHorizontalLine(0 + x, 52 + y, 128 + x);
}

//void drawNextDaysForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
//{
//  drawForecastDetails(display, x, y, 4);
//  drawForecastDetails(display, x + 44, y, 5);
//  drawForecastDetails(display, x + 88, y, 6);
//  drawHeaderOverlay(display, state);
//  ;
//}

// void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex)
// {
//   time_t observationTimestamp = forecasts[dayIndex].observationTime;
//   struct tm *timeInfo;
//   timeInfo = localtime(&observationTimestamp);
//   display->setTextAlignment(TEXT_ALIGN_CENTER);
//   display->setFont(ArialMT_Plain_10);
//   display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

//   display->setFont(Meteocons_Plain_21);
//   display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
//   String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
//   display->setFont(ArialMT_Plain_10);
//   display->drawString(x + 20, y + 34, temp);
//   display->setTextAlignment(TEXT_ALIGN_LEFT);
// }

// void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
// {
//   now = time(nullptr);
//   struct tm *timeInfo;
//   timeInfo = localtime(&now);
//   char buff[14];
//   sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

//   display->setColor(WHITE);
//   display->setFont(ArialMT_Plain_10);
//   display->setTextAlignment(TEXT_ALIGN_LEFT);
//   display->drawString(0, 54, String(buff));
//   display->setTextAlignment(TEXT_ALIGN_RIGHT);
//   String temp = String(currentWeatherData.temp, 1) + (IS_METRIC ? "°C" : "°F");
//   display->drawString(128, 54, temp);
//   display->drawHorizontalLine(0, 52, 128);
// }
