
#ifndef LIBSERIALIO_H
#define LIBSERIALIO_H
#include <Arduino.h>


/// convert given string to upper case
char *covertToUpper(char *str)
{
    char *p = str;
    while (*p != '\0')
    {
        *p = toupper(*p);
        p++;
    }
    
    return str;
}


/// try to read until line is idle for 10ms or more
inline void flush(Stream &_rSerial)
{
    for (unsigned long t = millis(); millis() < t + 10;)
    {
        if (_rSerial.available() > 0)
        {
            _rSerial.read();
            t = millis();
        }
    }
}


/// try to read until line is idle for 10ms or more, or until buffer is full
inline unsigned int read(Stream &_rSerial, char *_pBuf, unsigned int _uBufSize)
{
    unsigned int n = 0;
    for (unsigned long t = millis(); millis() < t + 10;)
    {
        if (_rSerial.available() > 0)
        {
            _pBuf[n] = _rSerial.read();
            n++;
                
            if (n >= _uBufSize)
            {
                break;
            }
                
            t = millis();
        }
    }
        
    return n;
}


/// try to read until line is idle for _uTimeOutMs, until buffer is full, or until NL or CR is received; returns immediately if no bytes available and _bCheckAvailable == true
inline unsigned int readln(Stream &_rSerial, char *_pBuf, unsigned int _uBufSize, unsigned long _uTimeOutMs, bool _bCheckAvailable)
{
	if ( (_bCheckAvailable == true) &&
		 (_rSerial.available() == 0) )
	{
		return 0;
	}
	
    unsigned int n = 0;
    for (unsigned long t = millis(); millis() < t + _uTimeOutMs;)
    {
        if (_rSerial.available() > 0)
        {
            int ch = _rSerial.read();
                
            // stop if NL or CR characters are received
            if ( (ch == '\n') ||
                 (ch == '\r') )
            {
                if (n > 0)  // ignores NL & CR if no other characters have been received
                {
                    break;
                }
            }
            else
            {
                _pBuf[n] = ch;
                n++;
                
                if (n >= _uBufSize)
                {
                    break;
                }
                else
                {
                    t = millis();
                }
            }
        }
    }
        
    return n;
}



#endif  // #ifndef LIBSERIALIO_H
