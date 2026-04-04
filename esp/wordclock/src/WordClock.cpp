#include "WordClock.h"
#include "SerialHelper.h"

const uint32_t COLORS[] = {
    0xFF0000,
    0x00FF00,
    0x0000FF,
    0xFFFF00,
    0xFF00FF,
    0x00FFFF,
    0xFFFFFF,
    0xA52A2A};

WordClock::WordClock(ClockDisplayHAL *clockDisplayHAL, NetworkManager *networkManager, GifPlayer *gifPlayer)
    : lastHour(-1), currentBrightness(DAY_BRIGHTNESS), allLastHighlightedWords(""), clockDisplayHAL(clockDisplayHAL), networkManager(networkManager), gifPlayer(gifPlayer), gifDownloaded(false) {}

void WordClock::setup()
{
    // Initialize WordClock by ensuring the GIF is available.
    SERIAL_PRINTLN("WordClock.setup: preparing GIF...");
    downloadGIF();
}

void WordClock::downloadGIF()
{
    if (gifDownloaded)
    {
        SERIAL_PRINTLN("WordClock.downloadGIF: GIF already loaded, skipping.");
        return;
    }

#ifdef ARDUINO_ARCH_ESP8266
    // On ESP8266 we rely on the locally flashed GIF to avoid fragile HTTPS downloads.
    if (networkManager->getGifBufferSize() == 0 || networkManager->getGifBuffer() == nullptr)
    {
        if (!networkManager->loadGIFFromSPIFFS("/heart_art_small.gif"))
        {
            SERIAL_PRINTLN("Failed to load GIF from SPIFFS.");
            return;
        }
    }

    uint8_t *gifBuffer = networkManager->getGifBuffer();
    size_t gifSize = networkManager->getGifBufferSize();
    if (gifSize > 0 && gifBuffer != nullptr && gifPlayer->loadGIF(gifBuffer, gifSize))
    {
        gifDownloaded = true;
        SERIAL_PRINT("GIF loaded from SPIFFS. Size=");
        SERIAL_PRINTLN(gifSize);
    }
    else
    {
        SERIAL_PRINTLN("Failed to load GIF from buffer on ESP8266.");
    }
#else
    const char *gifUrl = "https://raw.githubusercontent.com/johniak/word-clock/refs/heads/main/raspberry-pi/heart_art_small.gif";
    SERIAL_PRINTLN("WordClock.downloadGIF: attempting remote download...");
    if (networkManager->downloadGIF(gifUrl))
    {
        uint8_t *gifBuffer = networkManager->getGifBuffer();
        size_t gifSize = networkManager->getGifBufferSize();
        if (gifSize > 0 && gifBuffer != nullptr)
        {
            if (gifPlayer->loadGIF(gifBuffer, gifSize))
            {
                gifDownloaded = true;
                SERIAL_PRINT("GIF downloaded and loaded successfully. Size=");
                SERIAL_PRINTLN(gifSize);
            }
        }
    }
    else
    {
        SERIAL_PRINTLN("Failed to download GIF.");
    }
#endif
}

void WordClock::highlightWord(const String &word, uint32_t color)
{
    clockDisplayHAL->displayWord(word, color);
}

String WordClock::getMinutesWord(int minute)
{
    if (minute < 5)
        return "OCLOCK";
    else if (minute < 10)
        return "FIVE";
    else if (minute < 15)
        return "TEN";
    else if (minute < 20)
        return "FIFTEEN";
    else if (minute < 25)
        return "TWENTY";
    else if (minute < 30)
        return "TWENTYFIVE";
    else if (minute < 35)
        return "THIRTY";
    else if (minute < 40)
        return "TWENTYFIVE";
    else if (minute < 45)
        return "TWENTY";
    else if (minute < 50)
        return "FIFTEEN";
    else if (minute < 55)
        return "TEN";
    else
        return "FIVE";
}

uint32_t WordClock::getRandomColor()
{
    int index = random(0, sizeof(COLORS) / sizeof(COLORS[0]));
    return COLORS[index];
}

void WordClock::updateBrightness(int hour24)
{
    bool isNight = (hour24 >= NIGHT_START_HOUR || hour24 < NIGHT_END_HOUR);
    uint8_t target = isNight ? NIGHT_BRIGHTNESS : DAY_BRIGHTNESS;
    if (target != currentBrightness)
    {
        currentBrightness = target;
        clockDisplayHAL->setBrightness(target);
        SERIAL_PRINT("Brightness set to: ");
        SERIAL_PRINTLN(target);
    }
}

void WordClock::displayTime()
{
    struct tm currentTime = networkManager->getLocalTimeStruct();
    int hour24 = currentTime.tm_hour;
    int hour = hour24 % 12;
    if (hour == 0)
        hour = 12;
    int minute = currentTime.tm_min;

    updateBrightness(hour24);

    clockDisplayHAL->clearPixels(false);

    if (hour != lastHour && minute == 0)
    {
        lastHour = hour;
        if (gifDownloaded)
        {
            SERIAL_PRINT("Top of the hour. Playing GIF for hour ");
            SERIAL_PRINTLN(hour);
            gifPlayer->playGIF(4000);
        }
        clockDisplayHAL->clearPixels(false);
    }

    // Seed random from the current 5-minute interval so colors stay
    // stable within each interval and only change when the time changes.
    randomSeed(hour24 * 100 + minute / 5);

    highlightWord("IT", getRandomColor());
    highlightWord("IS", getRandomColor());
    String allHighlightedWords = "ITIS";

    if (minute < 5)
    {
        highlightWord("OCLOCK", getRandomColor());
        allHighlightedWords += "OCLOCK";
    }
    else if (minute < 35)
    {
        highlightWord("PAST", getRandomColor());
        highlightWord("MINUTES", getRandomColor());
        allHighlightedWords += "PASTMINUTES";
    }
    else
    {
        highlightWord("TO", getRandomColor());
        highlightWord("MINUTES", getRandomColor());
        allHighlightedWords += "TOMINUTES";
        hour = (hour + 1) % 12;
        if (hour == 0)
            hour = 12;
    }

    String hourWord = "HOUR_" + String(hour);
    highlightWord(getMinutesWord(minute), getRandomColor());
    allHighlightedWords += getMinutesWord(minute);
    highlightWord(hourWord, getRandomColor());
    allHighlightedWords += hourWord;

    if (allLastHighlightedWords != allHighlightedWords)
    {
        SERIAL_PRINT("Words: ");
        SERIAL_PRINTLN(allHighlightedWords);
        clockDisplayHAL->show();
        allLastHighlightedWords = allHighlightedWords;
    }
}
