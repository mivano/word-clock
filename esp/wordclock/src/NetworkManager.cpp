#include "NetworkManager.h"
#include "SerialHelper.h"

#ifdef ARDUINO_ARCH_ESP8266
    #include <LittleFS.h>
    #include <FS.h>
#endif
#include <stdlib.h> // setenv

#define MAX_GIF_SIZE 32768 // 32KB limit

NetworkManager::NetworkManager(const char *ssid, const char *password, long gmtOffset_sec, int daylightOffset_sec)
    : ssid(ssid), password(password), gmtOffset_sec(gmtOffset_sec), daylightOffset_sec(daylightOffset_sec), lastSyncTime(0), gifBuffer(nullptr), gifBufferSize(0) {}

void NetworkManager::setup()
{
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        SERIAL_PRINT(".");
    }
    SERIAL_PRINTLN("WiFi connected");

    syncTimeWithNTP();
}

void NetworkManager::update()
{
    unsigned long currentMillis = millis();
    if (currentMillis - lastSyncTime >= syncInterval)
    {
        syncTimeWithNTP();
    }
}

void NetworkManager::syncTimeWithNTP()
{
    const char *ntpServer = "pool.ntp.org";
    // Configure time sync with automatic DST if TIMEZONE is defined.
    #ifdef TIMEZONE
        configTime(0, 0, ntpServer);
        setenv("TZ", TIMEZONE, 1);
        tzset();
        SERIAL_PRINT("Timezone (TZ) set to: ");
        SERIAL_PRINTLN(TIMEZONE);
    #else
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        SERIAL_PRINT("Timezone offsets used. GMT: ");
        SERIAL_PRINT(gmtOffset_sec);
        SERIAL_PRINT(", DST: ");
        SERIAL_PRINTLN(daylightOffset_sec);
    #endif

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        SERIAL_PRINTLN("Failed to obtain time");
    }
    else
    {
        // Log a readable timestamp to verify timezone settings.
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        SERIAL_PRINT("Time sync complete: ");
        SERIAL_PRINTLN(buf);
    }
    lastSyncTime = millis();
}

struct tm NetworkManager::getLocalTimeStruct()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        return timeinfo;
    }
    else
    {
        memset(&timeinfo, 0, sizeof(struct tm));
        SERIAL_PRINTLN("Failed to obtain local time");
        return timeinfo;
    }
}

bool NetworkManager::downloadGIF(const char *gifUrl)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        SERIAL_PRINTLN("Attempting to download GIF from: ");
        SERIAL_PRINTLN(gifUrl);
        #ifdef ARDUINO_ARCH_ESP8266
            SERIAL_PRINT("Free heap: ");
            SERIAL_PRINTLN(ESP.getFreeHeap());
            SERIAL_PRINTLN("Note: HTTPS downloads are unstable on ESP8266 due to memory constraints.");
            SERIAL_PRINTLN("Consider using loadGIFFromSPIFFS() instead.");
        #endif
        
        HTTPClient http;
        #ifdef ARDUINO_ARCH_ESP8266
            BearSSL::WiFiClientSecure client;
            client.setInsecure();
            http.setTimeout(15000); // 15 second timeout
            http.begin(client, gifUrl);
            http.addHeader("User-Agent", "ESP8266-WordClock/1.0");
        #else
            http.begin(gifUrl);
        #endif

        SERIAL_PRINTLN("Sending GET request...");
        int httpResponseCode = http.GET();
        SERIAL_PRINT("HTTP Response Code: ");
        SERIAL_PRINTLN(httpResponseCode);
        
        if (httpResponseCode == HTTP_CODE_OK)
        {
            int gifSize = http.getSize();
            SERIAL_PRINT("GIF Size: ");
            SERIAL_PRINTLN(gifSize);

            if (gifSize > MAX_GIF_SIZE)
            {
                SERIAL_PRINTLN("GIF is too large. Max size allowed is 32KB.");
                http.end();
                return false;
            }

            gifBuffer = handleDownloadGIFResponse(http, gifSize);
            gifBufferSize = gifSize;
            http.end();
            return gifBuffer != nullptr;
        }
        else
        {
            SERIAL_PRINT("Failed to download GIF. HTTP Code: ");
            SERIAL_PRINTLN(httpResponseCode);
            http.end();
            return false;
        }
    }
    else
    {
        SERIAL_PRINT("WiFi not connected. Status: ");
        SERIAL_PRINTLN(WiFi.status());
        return false;
    }
}

uint8_t *NetworkManager::handleDownloadGIFResponse(HTTPClient &http, int gifSize)
{
    WiFiClient *stream = http.getStreamPtr();

    if (gifSize > 0)
    {
        SERIAL_PRINTLN("Downloading GIF...");

        uint8_t *buffer = (uint8_t *)malloc(gifSize);
        if (buffer == nullptr)
        {
            SERIAL_PRINTLN("Memory allocation failed for GIF");
            return nullptr;
        }

        int bytesRead = 0;
        while (http.connected() && stream->available() > 0 && bytesRead < gifSize)
        {
            int byte = stream->read();
            buffer[bytesRead++] = byte;
        }

        SERIAL_PRINTLN("GIF downloaded and stored in memory");
        return buffer;
    }
    else
    {
        SERIAL_PRINTLN("No data available for GIF");
        return nullptr;
    }
}

uint8_t *NetworkManager::getGifBuffer()
{
    return gifBuffer;
}

size_t NetworkManager::getGifBufferSize()
{
    return gifBufferSize;
}
bool NetworkManager::loadGIFFromSPIFFS(const char *filePath)
{
    #ifdef ARDUINO_ARCH_ESP8266
        if (!fsMounted && !LittleFS.begin())
        {
            SERIAL_PRINTLN("Failed to mount LittleFS");
            return false;
        }
        fsMounted = true;
        // Log filesystem capacity and usage when mounted to help diagnose storage issues.
        FSInfo fs_info;
        if (LittleFS.info(fs_info))
        {
            SERIAL_PRINT("LittleFS total bytes: ");
            SERIAL_PRINTLN(fs_info.totalBytes);
            SERIAL_PRINT("LittleFS used bytes: ");
            SERIAL_PRINTLN(fs_info.usedBytes);
        }
        
        SERIAL_PRINT("Looking for file: ");
        SERIAL_PRINTLN(filePath);
        
        // List files in root to debug
        Dir dir = LittleFS.openDir("/");
        SERIAL_PRINTLN("Files in SPIFFS:");
        while (dir.next())
        {
            SERIAL_PRINT("  ");
            SERIAL_PRINTLN(dir.fileName());
        }
        
        File file = LittleFS.open(filePath, "r");
        if (!file)
        {
            SERIAL_PRINTLN("Failed to open GIF file from SPIFFS");
            return false;
        }
        
        size_t fileSize = file.size();
        SERIAL_PRINT("File size: ");
        SERIAL_PRINTLN(fileSize);
        
        if (fileSize > MAX_GIF_SIZE)
        {
            SERIAL_PRINTLN("GIF file too large");
            file.close();
            return false;
        }
        
        freeGifBuffer();
        SERIAL_PRINT("Free heap before GIF alloc: ");
        SERIAL_PRINTLN(ESP.getFreeHeap());
        gifBuffer = (uint8_t *)malloc(fileSize);
        if (!gifBuffer)
        {
            SERIAL_PRINTLN("Memory allocation failed for GIF");
            file.close();
            return false;
        }
        SERIAL_PRINT("Free heap after GIF alloc: ");
        SERIAL_PRINTLN(ESP.getFreeHeap());
        
        size_t bytesRead = file.read(gifBuffer, fileSize);
        file.close();
        
        if (bytesRead != fileSize)
        {
            SERIAL_PRINTLN("Failed to read complete GIF file");
            free(gifBuffer);
            gifBuffer = nullptr;
            return false;
        }
        
        gifBufferSize = fileSize;
        SERIAL_PRINT("Loaded GIF from SPIFFS: ");
        SERIAL_PRINTLN(fileSize);
        return true;
    #else
        SERIAL_PRINTLN("SPIFFS loading not supported on ESP32");
        return false;
    #endif
}

void NetworkManager::freeGifBuffer()
{
    // Release any previously allocated GIF buffer to avoid leaks.
    if (gifBuffer != nullptr)
    {
        free(gifBuffer);
        gifBuffer = nullptr;
        gifBufferSize = 0;
    }
}