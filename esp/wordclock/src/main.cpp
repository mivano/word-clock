#include <Arduino.h>
#include "ClockDisplayHAL.h"
#include "NetworkManager.h"
#include "SerialHelper.h"
#include "config.h"
#include "GifPlayer.h"
#include "WordClock.h"

#ifdef ARDUINO_ARCH_ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif

NetworkManager networkManager(WIFI_SSID, WIFI_PASSWORD, GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC);
ClockDisplayHAL clockDisplayHAL(LED_PIN, 255);
GifPlayer gifPlayer(&clockDisplayHAL);
WordClock wordClock(&clockDisplayHAL, &networkManager, &gifPlayer);

void setup()
{
  initSerial();
  // Log basic startup context and architecture for debugging.
  SERIAL_PRINTLN("WordClock setup starting...");
#ifdef ARDUINO_ARCH_ESP8266
  SERIAL_PRINTLN("Architecture: ESP8266 (D1 Mini)");
#else
  SERIAL_PRINTLN("Architecture: ESP32");
#endif
  networkManager.setup();
  SERIAL_PRINT("WiFi IP: ");
  SERIAL_PRINTLN(WiFi.localIP());

  clockDisplayHAL.setup();
  SERIAL_PRINTLN("Display initialized.");
  
  // Load GIF from storage (SPIFFS for ESP8266, or download for ESP32)
  #ifdef ARDUINO_ARCH_ESP8266
    SERIAL_PRINTLN("Loading GIF from SPIFFS...");
    if (!networkManager.loadGIFFromSPIFFS("/heart_art_small.gif"))
    {
      SERIAL_PRINTLN("Failed to load GIF from SPIFFS");
    }
    else
    {
      SERIAL_PRINT("GIF buffer size: ");
      SERIAL_PRINTLN(networkManager.getGifBufferSize());
    }
  #else
    SERIAL_PRINTLN("Downloading GIF from remote URL...");
    if (!networkManager.downloadGIF("https://raw.githubusercontent.com/johniak/word-clock/refs/heads/main/raspberry-pi/heart_art_small.gif"))
    {
      SERIAL_PRINTLN("Failed to download GIF");
    }
    else
    {
      SERIAL_PRINT("GIF buffer size: ");
      SERIAL_PRINTLN(networkManager.getGifBufferSize());
    }
  #endif
  
  wordClock.setup();
  SERIAL_PRINTLN("WordClock setup complete.");
}

void loop()
{
  networkManager.update();
  wordClock.displayTime();
  delay(1000);
}
