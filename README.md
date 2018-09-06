# ESP32 Weather Station
ESP32 weather station with DHT22 temperature / humidity sensor and SSD1306 display.

Overall price is about £15

Video: https://www.youtube.com/watch?v=bA8N2sMjCUM

## Instructions

### What you should already have
1. Arduino IDE: https://www.arduino.cc/en/Guide/HomePage
2. ESP32 OLED: https://www.amazon.co.uk/MakerHawk-Development-Bluetooth-Module-Arduino/dp/B071G2MYX2/ref=sr_1_3?ie=UTF8&qid=1536170567&sr=8-3&keywords=esp32%2Boled&th=1
3. Temperature and humidity sensor DHT22: https://www.amazon.co.uk/Digital-Temperature-Humidity-Measure-Arduino/dp/B06WLKQMHY/ref=sr_1_7?s=computers&ie=UTF8&qid=1536170623&sr=1-7&keywords=DHT22
4. USB cable to connect the ESP32 to your PC.
5. Soldering iron (station)
6. Spool of solder
7. 3 wires to connect the DHT22 to ESP32

### Soldering
- Solder `+` pin of DHT22 to 3.3V pin on ESP32
- Solder `-` pin of DHT22 to GND pin on ESP32
- Solder `out` pin of DHT22 to pin number 16 on ESP32

### Install all required Arduino libraries
How to install arduino library: https://www.arduino.cc/en/Guide/Libraries

The libraries we need.
- Time library: https://github.com/PaulStoffregen/Time
- DHT Sensor Library for ESP32: https://github.com/beegee-tokyo/DHTesp
- SSD1306 display library: https://github.com/ThingPulse/esp8266-oled-ssd1306
- esp8266-weather-station: https://github.com/gandalf15/esp8266-weather-station

### Add ESP32 as your board
- Check out this link: https://github.com/espressif/arduino-esp32
- More specifically this section: https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md


### Clone
clone this repository to your Arduino projects folder with command:

$ `git clone https://github.com/gandalf15/esp32_weather_station.git`

### Edit
1. Open esp32_weather_station.ino project in Arduino IDE
2. set up your WiFi SSID (name)
3. set up your WiFi password
4. Sign up here to get an API key: https://docs.thingpulse.com/how-tos/openweathermap-key/
5. set up your API key
6. find your city ID here: http://bulk.openweathermap.org/sample/city.list.json.gz
7. set up your city ID

### Compile and Upload
1. Connect your ESP32 via the USB cable to your computer and click on the upload icon.

### Done
