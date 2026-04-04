#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#ifdef ARDUINO_ARCH_ESP8266
    #include <ESP8266WiFi.h>
    #include <ESP8266HTTPClient.h>
    #include <WiFiClientSecure.h>
    #include <user_interface.h>
#else
    #include <WiFi.h>
    #include <HTTPClient.h>
#endif
#include <time.h>

class NetworkManager
{
public:
    NetworkManager(const char *ssid, const char *password, long gmtOffset_sec, int daylightOffset_sec);
    void setup();
    void update();
    struct tm getLocalTimeStruct();
    bool downloadGIF(const char *gifUrl);
    bool loadGIFFromSPIFFS(const char *filePath);
    uint8_t *getGifBuffer();
    size_t getGifBufferSize();

private:
    const char *ssid;
    const char *password;
    long gmtOffset_sec;
    int daylightOffset_sec;
    unsigned long lastSyncTime;
    unsigned long lastReconnectAttempt;
    const unsigned long syncInterval = 86400000;
    const unsigned long reconnectInterval = 30000; // retry WiFi every 30s
    uint8_t *gifBuffer = nullptr;
    size_t gifBufferSize = 0;
    bool fsMounted = false;

    void syncTimeWithNTP();
    uint8_t *handleDownloadGIFResponse(HTTPClient &http, int gifSize);
    void freeGifBuffer();
};

#endif
