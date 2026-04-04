#ifndef WORDCLOCK_H
#define WORDCLOCK_H

#include <Arduino.h>
#include "ClockDisplayHAL.h"
#include "NetworkManager.h"
#include "GifPlayer.h"
#include "config.h"

class WordClock
{
public:
    WordClock(ClockDisplayHAL *clockDisplayHAL, NetworkManager *networkManager, GifPlayer *gifPlayer);
    void setup();
    void displayTime();

private:
    int lastHour;
    uint8_t currentBrightness;
    String allLastHighlightedWords;
    ClockDisplayHAL *clockDisplayHAL;
    NetworkManager *networkManager;
    GifPlayer *gifPlayer;
    bool gifDownloaded;

    void downloadGIF();
    void updateBrightness(int hour24);
    void highlightWord(const String &word, uint32_t color = 0xFFFFFF);
    String getMinutesWord(int minute);
    uint32_t getRandomColor();
};

#endif
