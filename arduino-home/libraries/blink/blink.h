
#ifndef BLINK_H
#define BLINK_H
#include <Arduino.h>


/// will blink the given pin at the given rate (function keeps state and should be used for only one pin)
void blink(unsigned long _uTimeMs, int _iPin)
{
    static unsigned long uLoopExpireTime = 0;
    static bool bLoopLedOn = false;
    
    if (millis() > uLoopExpireTime)
    {
        uLoopExpireTime = millis() + _uTimeMs;
        bLoopLedOn = !bLoopLedOn;
        digitalWrite(_iPin, bLoopLedOn ? HIGH : LOW);
    }
}



/// will blink the given pin at the given rate (function keeps state and should be used for only one pin)
void flash(unsigned long _uTimeMs, int _iPin)
{
    static unsigned long uLoopExpireTime = 0;
    static bool bLoopLedOn = false;
    
    if (millis() > uLoopExpireTime)
    {
        uLoopExpireTime = millis() + (bLoopLedOn ? _uTimeMs : _uTimeMs/20);
        bLoopLedOn = !bLoopLedOn;
        digitalWrite(_iPin, bLoopLedOn ? HIGH : LOW);
    }
}


void waitAndFlash(unsigned long _uTimeToWaitMs, unsigned long _uFlashTime, int _iPin)
{
    const unsigned long t = millis() + _uTimeToWaitMs;
    while (millis() < t)
    {
        flash(_uFlashTime, _iPin);
    }
}


void waitAndBlink(unsigned long _uTimeToWaitMs, unsigned long _uBlinkTime, int _iPin)
{
    const unsigned long t = millis() + _uTimeToWaitMs;
    while (millis() < t)
    {
        blink(_uBlinkTime, _iPin);
    }
}


void waitOnOff(unsigned long _uTimeToWaitMs, int _iPin)
{
    digitalWrite(_iPin, HIGH);
    delay(_uTimeToWaitMs);
    digitalWrite(_iPin, LOW);
}


#endif  // #ifndef BLINK_H

