// Creating using Chatgpt and some code for weather from this amazing project: https://github.com/G6EJD/ESP32-e-Paper-Weather-Display//


#include "WiFi.h"
#include "HTTPClient.h"
#include "ArduinoJson.h"
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "time.h"
#include <tinyxml2.h>
using namespace tinyxml2;
// Constants
#define uS_TO_S_FACTOR 1000000ULL // Conversion factor for microseconds to seconds
#define DEEP_SLEEP_DURATION 600   // Sleep duration in seconds (10 minutes)
#define RSS_FETCH_INTERVAL 28      // Fetch RSS every 6 wake-ups (4.6 hour)
#define CALENDAR_FETCH_INTERVAL 18 // Fetch calendar every 18 wake-ups (3 hours)
#define WAKEUP_TIME 7  // 7:00 AM
#define SLEEP_TIME 1  // 1:00 AM

// Persistent variables using RTC memory
RTC_DATA_ATTR int currentTitleIndex = 0; // Tracks the index of the news title
RTC_DATA_ATTR int wakeUpCount = 0;       // Tracks wake-up cycles for RSS fetch
RTC_DATA_ATTR int newsCount = 0;  // Store the number of news titles in RTC memory
RTC_DATA_ATTR char newsTitles[28][100] = {{0}}; // Store news titles directly in RTC memory
RTC_DATA_ATTR char calendarPayload[180] = {0};

// WiFi credentials
#define WIFI_SSID "xx.xx"
#define WIFI_PASSWORD "xxxxx"

boolean LargeIcon = true, SmallIcon = false;
#define Large  9           // For icon drawing, needs to be odd number for best effect
#define Small  5            // For icon drawing, needs to be odd number for best effect


// OpenWeatherMap API
#define URL "http://api.openweathermap.org/data/2.5/weather?xxxxxxxxx"

const char* CALENDAR_URL = "https://script.google.com/macros/s/xxxxxxA/exec";

const char* RSS_URL = "https://feeds.bbci.co.uk/news/world/rss.xml";

// ePaper display setup
//1 BUSY	GPIO4
//2 RES	GPIO2
//3 D/C	GPIO3
//4 CS	GPIO5
//5 SCL	GPIO12
//6 SDA	GPIO11
//7 GND	GND
//8 VCC	3V3

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(/*CS=*/ 5, /*DC=*/ 3, /*RES=*/ 2, /*BUSY=*/ 4));

StaticJsonDocument<8192> doc;
HTTPClient http;



void setTimezone(String timezone) {
  Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);
  tzset();
}

void initTime(String timezone) {
  struct tm timeinfo;
  Serial.println("Setting up time");
  configTime(0, 0, "pool.ntp.org");
  if (!getLocalTime(&timeinfo)) {
    Serial.println("  Failed to obtain time");
    return;
  }
  Serial.println("  Got the time from NTP");
  setTimezone(timezone);
}

String getLocalTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "Time N/A";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%a %d %b %H:%M", &timeinfo);
  return String(timeStringBuff);
}

void wifi_init() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  initTime("GMT0BST,M3.5.0/1,M10.5.0");
}

void downloadWeather() {
  http.begin(URL);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String json = http.getString();
   // Serial.println("Raw JSON data:");
   // Serial.println(json);
    int startIndex = json.indexOf('{');
    if (startIndex != -1) {
      json = json.substring(startIndex);
    }
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
      Serial.printf("Deserialization failed: %s\n", error.c_str());
    } else {
      Serial.println("JSON parsed successfully!");
    }
  } else {
    Serial.printf("HTTP GET Request failed with code: %d\n", httpResponseCode);
  }
  http.end();
}
void DisplayWXicon(int x, int y, String IconName, bool IconSize) {
  Serial.println(IconName);
  if      (IconName == "01d" || IconName == "01n")  Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")  MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")  Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")  MostlyCloudy(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);
  else if (IconName == "50d")                       Haze(x, y, IconSize, IconName);
  else if (IconName == "50n")                       Fog(x, y, IconSize, IconName);
  else                                              Nodata(x, y, IconSize, IconName);
}

//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                      // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                      // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);            // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE); // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
  x = x + scale * 1.6; y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 4; d++) {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 3;
  if (IconSize == SmallIcon) linesize = 1;
  display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_BLACK);
  display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  if (IconSize == LargeIcon) {
    display.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  }
  display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) {
    y -= 10;
    linesize = 1;
  }
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, GxEPD_BLACK);
  }
}
//#########################################################################################
void Sunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small, offset = 3;
  if (IconSize == LargeIcon) {
    scale = Large;
    y = y - 8;
    offset = 18;
  } else y = y - 3; // Shift up small sun icon
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  scale = scale * 1.6;
  addsun(x, y, scale, IconSize);
}
//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset = 3;
  if (IconSize == LargeIcon) {
    scale = Large;
    offset = 10;
  } else linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    linesize = 1;
    addcloud(x, y, scale, linesize);
  }
  else {
    y += 10;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    addcloud(x + 30, y - 35, 5, linesize); // Cloud top right
    addcloud(x - 20, y - 25, 7, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);       // Main cloud
  }
}
//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y + 10, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y + 15, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale, IconSize);
}
//#########################################################################################
void Fog(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y - 5, scale, linesize);
  addfog(x, y - 5, scale, linesize, IconSize);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x, y - 5, scale * 1.4, IconSize);
  addfog(x, y - 5, scale * 1.4, linesize, IconSize);
}

//#########################################################################################
void addmoon(int x, int y, int scale, bool IconSize) {
  if (IconSize == LargeIcon) {
    x = x + 12; y = y + 12;
    display.fillCircle(x - 50, y - 55, scale, GxEPD_BLACK);
    display.fillCircle(x - 35, y - 55, scale * 1.6, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 20, y - 12, scale, GxEPD_BLACK);
    display.fillCircle(x - 15, y - 12, scale * 1.6, GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) display.setFont(&FreeMono9pt7b); else display.setFont(&FreeMono9pt7b);
  display.print("?");
  display.setFont(&FreeMono9pt7b);
}

// Bitmaps for the icons
static const unsigned char PROGMEM image_weather_temperature_bits[] = {
    0x1c, 0x00, 0x22, 0x02, 0x2b, 0x05, 0x2a, 0x02, 0x2b, 0x38, 0x2a, 0x60,
    0x2b, 0x40, 0x2a, 0x40, 0x2a, 0x60, 0x49, 0x38, 0x9c, 0x80, 0xae, 0x80,
    0xbe, 0x80, 0x9c, 0x80, 0x41, 0x00, 0x3e, 0x00};

static const unsigned char PROGMEM image_weather_humidity_white_bits[] = {
    0x04, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x0a, 0x00, 0x12, 0x00, 0x11, 0x00,
    0x20, 0x80, 0x20, 0x80, 0x41, 0x40, 0x40, 0xc0, 0x80, 0xa0, 0x80, 0x20,
    0x40, 0x40, 0x40, 0x40, 0x30, 0x80, 0x0f, 0x00};

void DrawBattery(int x, int y) {
    uint8_t percentage = 100; // Default percentage
    float voltage = analogRead(7) / 4096.0 * 7.46; // Convert ADC value to voltage (adjust multiplier as per your voltage divider)

    if (voltage > 1) { // Check for a valid reading
        Serial.println("Voltage = " + String(voltage, 2) + "V");

        // Calculate battery percentage using the polynomial
        percentage = 2836.9625 * pow(voltage, 4) - 
                     43987.4889 * pow(voltage, 3) + 
                     255233.8134 * pow(voltage, 2) - 
                     656689.7123 * voltage + 
                     632041.7303;

        // Clamp percentage to valid range
        if (voltage >= 4.20) percentage = 100;
        if (voltage <= 3.50) percentage = 0;

        // Draw battery outline and fill it according to the percentage
        display.drawRect(x, y, 19, 10, GxEPD_BLACK); // Adjusted to fit in the top-right corner
        display.fillRect(x + 19, y + 2, 2, 6, GxEPD_BLACK);
        display.fillRect(x + 2, y + 2, 15 * percentage / 100.0, 6, GxEPD_BLACK);

        // Display percentage text
        String percentBattery = String(percentage) + "%";
        display.setFont(&FreeMono9pt7b);
        display.setCursor(x + 25, y + 8); // Positioned next to the battery icon
        display.print(percentBattery);
    }
}


void displayWeatherAndTime() {
    display.setPartialWindow(0, 0, display.width(), 95); // Top 95px for weather/time
    display.firstPage();
    do {
        // Clear the weather/time area
        display.fillRect(0, 0, display.width(), 95, GxEPD_WHITE);

        // Get weather details
        const char* weather = doc["weather"][0]["description"];
        const char* iconCode = doc["weather"][0]["icon"];
        float temp = doc["main"]["temp"];
        int hum = doc["main"]["humidity"];

        // Get current time
        String newTime = getLocalTimeString();

        // Draw time in top left
        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(5, 10);
        display.print(newTime);

        // Draw battery in top right
        DrawBattery(display.width() - 70, 3);
        // Horizontal line below time/battery
        display.drawLine(0, 15, display.width(), 15, GxEPD_BLACK);

        // Define the rectangle dimensions and layout
        int rectHeight = 60;  // Height of each rectangle
        int rectWidth = display.width() / 3;  // Divide the width equally into three parts
        int rectTop = 20;  // Start position just below the horizontal line

       // Left rectangle: Temperature and Humidity
        //display.drawRect(0, rectTop, rectWidth, rectHeight, GxEPD_BLACK);
        display.setFont(&FreeSansBold12pt7b);
      // Temperature
        int tempX = 35; // Move this further to the right
        int tempY = rectTop + 25;
        display.setCursor(tempX, tempY);
        display.print(String(temp, 1));
        display.fillCircle(display.getCursorX() + 3, display.getCursorY() - 10, 2, GxEPD_BLACK);  // Draw degree symbol
        display.print(" °C");;
        display.drawBitmap(tempX - 20, tempY - 14, image_weather_temperature_bits, 16, 16, GxEPD_BLACK);

            // Humidity
        int humX = 35; // Move this further to the right
        int humY = tempY + 30;
        display.setCursor(humX, humY);
        display.print(String(hum) + "%");
        display.drawBitmap(humX - 20, humY - 14, image_weather_humidity_white_bits, 16, 16, GxEPD_BLACK);

            // Middle rectangle: Weather Icon
       // display.drawRect(rectWidth, rectTop, rectWidth, rectHeight, GxEPD_BLACK);
        int iconX = rectWidth + rectWidth / 2 - 25 + 15;  // Move the icon 10 pixels to the right
        int iconY = rectTop + rectHeight / 2 - 20 + 40;  // Move the icon 40 pixels down
        DisplayWXicon(iconX, iconY, String(iconCode), LargeIcon);
   
      // Right rectangle: Weather Condition
      //display.drawRect(2 * rectWidth, rectTop, rectWidth, rectHeight, GxEPD_BLACK);
      display.setFont(&FreeMono12pt7b);

      int conditionX = 2 * rectWidth + 10;  // Add some padding inside the rectangle
      int conditionY = rectTop + 30;        // Start position for the first line

      String weatherCondition = weather ? weather : "N/A";

      // Split long conditions into two lines if necessary
      if (weatherCondition.length() > 8) {  // 8 characters per line
    int splitIndex = 8;  // Max characters before splitting

    // Find the last space before the 8th character to split the string
    while (splitIndex > 0 && weatherCondition[splitIndex] != ' ') {
        splitIndex--;
    }

    // If no space is found, split at the 8th character anyway
    if (splitIndex == 0) {
        splitIndex = 7;
    }

    String firstLine = weatherCondition.substring(0, splitIndex);
    String secondLine = weatherCondition.substring(splitIndex + 1);  // +1 to skip the space

    // Print the first line inside the rectangle
    display.setCursor(conditionX, conditionY);
    display.print(firstLine);

    // Print the second line just below the first line, staying inside the rectangle
    display.setCursor(conditionX, conditionY + 20);
    display.print(secondLine);
} else {
    // If no wrapping is needed, print the condition in the center of the rectangle
    display.setCursor(conditionX, conditionY);
    display.print(weatherCondition);
}
    } while (display.nextPage());
}


// Function to fetch and display calendar events
void fetchAndDisplayCalendar() {
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(CALENDAR_URL); // HTTPS connection

    int httpCode = http.GET();
    if (httpCode > 0) {
        String payload = http.getString();
        Serial.printf("Calendar fetched successfully. HTTP code: %d\n", httpCode);
        Serial.println("Payload:");
        Serial.println(payload);

        // Save to RTC memory
        if (payload.length() < sizeof(calendarPayload)) {
            payload.toCharArray(calendarPayload, sizeof(calendarPayload));
        } else {
            Serial.println("Calendar payload too large, truncating.");
            payload.substring(0, sizeof(calendarPayload) - 1).toCharArray(calendarPayload, sizeof(calendarPayload));
        }

        // Display calendar
        displayEvents(payload);
    } else {
        Serial.printf("Failed to fetch calendar. HTTP code: %d\n", httpCode);
    }
    http.end();
}

void displaySavedCalendar() {
    String payload = String(calendarPayload);
    displayEvents(payload);
}

void displayEvents(String serverPayload) {
    display.setFont(&FreeMonoBold9pt7b); // Set font for the calendar section
    display.setTextColor(GxEPD_BLACK);
    display.setPartialWindow(0, 100, display.width(), 140); // Middle section: y=100 to y=240
    display.firstPage();
    do {
        display.fillRect(0, 100, display.width(), 140, GxEPD_WHITE); // Clear the calendar area
 
        char buff[serverPayload.length() + 1];
        serverPayload.toCharArray(buff, serverPayload.length() + 1);

        char* line = strtok(buff, "\n"); // Split payload into lines
        int y = 120; // Start position for the events section

        for (int i = 0; line != NULL && i < 5; i++) { // Limit to 5 lines
            display.setCursor(10, y + (16 * i)); // Adjust spacing between lines
            display.printf("%.30s", line);      // Print up to 30 characters per line
            line = strtok(NULL, "\n");
        }
    } while (display.nextPage());
}

// Function to fetch and parse RSS feed
String fetchLatestNews() {
    HTTPClient http;
    http.begin(RSS_URL);
    int httpCode = http.GET();

    if (httpCode > 0) {
        String payload = http.getString();
        XMLDocument rssFeed;
        if (rssFeed.Parse(payload.c_str()) == XML_SUCCESS) {
            XMLElement* channel = rssFeed.FirstChildElement("rss")->FirstChildElement("channel");
            if (channel) {
                XMLElement* item = channel->FirstChildElement("item");
                newsCount = 0; // Reset title count
                while (item && newsCount < 28) {
                    XMLElement* title = item->FirstChildElement("title");
                    if (title && title->GetText()) {
                        // Directly copy the title to the RTC memory array
                        strncpy(newsTitles[newsCount], title->GetText(), sizeof(newsTitles[newsCount]) - 1);
                        newsTitles[newsCount][sizeof(newsTitles[newsCount]) - 1] = '\0'; // Ensure null-termination
                        newsCount++;
                    }
                    item = item->NextSiblingElement("item");
                }
                Serial.println("RSS Feed updated!");
                return "Success";
            }
        } else {
            Serial.println("Failed to parse RSS feed.");
            return "Parse error";
        }
    } else {
        Serial.printf("Failed to fetch RSS feed. HTTP code: %d\n", httpCode);
        return "HTTP error";
    }
    http.end();
    return "Unknown error";
}

// Function to display the news at the bottom of the screen
void displayNews(int titleIndex) {
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setPartialWindow(0, 240, display.width(), 60); // Bottom section: y=240 to y=300
    display.firstPage();
    do {
        display.fillRect(0, 240, display.width(), 60, GxEPD_WHITE); // Clear news area

       if (titleIndex >= 0 && titleIndex < newsCount) {
    int y = 250; // Adjust y position for news
    display.setCursor(10, y);
    
    // Get the title for the current index
    String title = String(newsTitles[titleIndex]);

    // Split into multiple lines if necessary
    if (title.length() > 35) {
        int splitIndex1 = 34;

        // Find the last space before the 35th character for the first line
        while (splitIndex1 > 0 && title[splitIndex1] != ' ') {
            splitIndex1--;
        }
        if (splitIndex1 == 0) {
            splitIndex1 = 34; // Split at the 35th character if no space is found
        }

        String firstLine = title.substring(0, splitIndex1);
        display.print(firstLine);
        
        // Adjust cursor for the second line
        display.setCursor(10, y + 20);

        String remainingText = title.substring(splitIndex1 + 1); // +1 to skip the space
        
        if (remainingText.length() > 35) {
            int splitIndex2 = 34;

            // Find the last space before the 35th character for the second line
            while (splitIndex2 > 0 && remainingText[splitIndex2] != ' ') {
                splitIndex2--;
            }
            if (splitIndex2 == 0) {
                splitIndex2 = 34; // Split at the 35th character if no space is found
            }

            String secondLine = remainingText.substring(0, splitIndex2);
            String thirdLine = remainingText.substring(splitIndex2 + 1); // +1 to skip the space
            display.print(secondLine);

            // Adjust cursor for the third line
            display.setCursor(10, y + 40);
            display.print(thirdLine);
        } else {
            // If the remaining text fits on the second line
            display.print(remainingText);
        }
    } else {
        // If the title fits on a single line
        display.print(title);
    }
} else {
    display.setCursor(10, 250);
    display.print("No news available");
}

    } while (display.nextPage());
}




void setup() {
    Serial.begin(115200);
    delay(100);           // Allow some time for the connection to stabilize
    Serial.println("Starting setup...");
    display.init(115200, true, 50, false); // Initialize display
    wifi_init(); // Initialize Wi-Fi
  // Check current time
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time. Going to sleep for default duration.");
        ESP.deepSleep(DEEP_SLEEP_DURATION * uS_TO_S_FACTOR);
        return;
    }

    int currentHour = timeinfo.tm_hour;
    Serial.printf("Current Hour: %d\n", currentHour);

    // Restrict operation based on time
    if (currentHour < WAKEUP_TIME && currentHour >= SLEEP_TIME) {
        // Calculate sleep duration until WAKEUP_TIME
        int sleepHours = WAKEUP_TIME - currentHour;
        unsigned long long extendedSleep = sleepHours * 3600 * uS_TO_S_FACTOR;
        Serial.printf("Outside operational hours. Sleeping for %d hours.\n", sleepHours);
        ESP.deepSleep(extendedSleep);
        return;
    }

    // Proceed with normal operation
    Serial.println("Within operational hours. Proceeding with tasks...");
    
    if (wakeUpCount == 0 || wakeUpCount % RSS_FETCH_INTERVAL == 0) {
        fetchLatestNews(); // Fetch RSS feed on first boot or every 1 hour
    }

    if (newsCount > 0) {
        displayNews(currentTitleIndex); // Display the current news title
        currentTitleIndex = (currentTitleIndex + 1) % newsCount; // Cycle to the next title
    } else {
        displayNews(-1); // Display "No news available"
    }

 // Handle Calendar
    if (wakeUpCount == 0 || wakeUpCount % CALENDAR_FETCH_INTERVAL == 0) {
        fetchAndDisplayCalendar(); // Fetch and display calendar every 3 hours
    } else {
        displaySavedCalendar(); // Display previously fetched calendar
    }

    // Display weather and calendar
    downloadWeather();
  
    displayWeatherAndTime();

    // Prepare for deep sleep
    wakeUpCount++;
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * uS_TO_S_FACTOR); // Configure sleep time
    Serial.println("Going to deep sleep...");
    display.hibernate(); // Hibernate the display for battery saving
    esp_deep_sleep_start();
}


void loop() {

}

