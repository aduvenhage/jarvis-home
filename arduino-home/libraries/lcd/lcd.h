#ifndef LCD_H
#define LCD_H
#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>


/// Round up to next higher power of 2 (return x if it's already a power of 2).
size_t pow2ceil(size_t i)
{
    --i;
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    
    return i+1;
}


/**
 class for maintaining scrolling text on a LCD
*/
class LcdScreen
{
  public:
    LcdScreen(Stream &_serial, unsigned int _uWidth, unsigned int _uHeight)
        :m_serial(_serial),
         m_uBufWidth(0),
         m_uBufHeightMask(0),
         m_pBuffer(NULL),
         m_uBufferSize(0),
         m_uLineCount(0),
         m_iX(0),
         m_iY(0),
         m_bBlink(false),
         m_uBlinkMillis(0)
    {
        m_uBufWidth = max(16, _uWidth);
        m_uBufHeightMask = max(4, pow2ceil(_uHeight))-1;
        
        m_uBufferSize = (unsigned long)m_uBufWidth * (unsigned long)(m_uBufHeightMask+1);
        m_pBuffer = (char*)malloc(m_uBufferSize);
        memset(m_pBuffer, ' ', m_uBufferSize);
        
        setup();
    }

    virtual ~LcdScreen()
    {
        free(m_pBuffer);
    }

    /// setup LCD
    void setup()
    {
        m_serial.write(0x7C); // set LCD width (1)
        m_serial.write(4);    // set LCD width (2)
        m_serial.write(0x7C); // set LCD height (1)
        m_serial.write(6);    // set LCD height (1)
        delay(200);
        m_serial.write(0xFE); // switch cursor off (1)
        m_serial.write(0x0C); // switch cursor off (2)
        delay(200);
        m_serial.write(0xFE); // clear LCD (1)
        m_serial.write(0x01); // clear LCD (2)
        delay(200);
        m_serial.write(0x7C); // set LCD brightness (1)
        m_serial.write(140);  // set LCD brightness (2 : 128 - off; 157 - fully on)
        delay(20);
    }
    
    /// sets the backlight brightness (0 - 29)
    void setBacklight(unsigned char _uValue)
    {
        m_serial.write(0x7C);           // set LCD brightness (1)
        m_serial.write(128 + _uValue);  // set LCD brightness (2 : 128 - off; 157 - fully on)
        delay(20);
    }
    
    /// update text on LCD based on x and y coordinates
    void scroll(int _ix, int _iy)
    {
        if (_ix < 0) m_iX = 0;
        else if (_ix > m_uBufWidth-16) m_iX = m_uBufWidth-16;
        else m_iX = _ix;
        
        if (_iy < 0) m_iY = 0;
        else if (_iy > m_uBufHeightMask-1) m_iY = m_uBufHeightMask-1;
        else m_iY = _iy;
        
        // display bottom two lines
        unsigned int i1 = (m_uLineCount-2u-m_iY) & m_uBufHeightMask;
        unsigned int i2 = (m_uLineCount-1u-m_iY) & m_uBufHeightMask;
        
        // write line 1
        m_serial.write(0xFE);
        m_serial.write(0x80);
        m_serial.write((unsigned char*)m_pBuffer + (unsigned long)i1 * m_uBufWidth + (unsigned long)m_iX, 16);
        delay(50);
        
        // write line 2
        m_serial.write(0xFE);
        m_serial.write(0x80 | 0x40);
        m_serial.write((unsigned char*)m_pBuffer + (unsigned long)i2 * m_uBufWidth + (unsigned long)m_iX, 16);
        delay(50);
    }
    
    /// write a new line to LCD and scrolls previous line up
	void writeLine(const char *_pszFmt, ...)
	{
        // clear line
        char *pLcdBufLine = m_pBuffer + (unsigned long)(m_uLineCount & m_uBufHeightMask) * m_uBufWidth;
        memset(pLcdBufLine, ' ', m_uBufWidth);

        // fill line with variable format string
		va_list ap;
		va_start(ap, _pszFmt);
        int n = vsnprintf(pLcdBufLine, m_uBufWidth, _pszFmt, ap);
		va_end(ap);
        
        // replace string terminating character '\0' with a ' '
        n = min(m_uBufWidth-1, n);
        pLcdBufLine[n] = ' ';
        
        // scroll text
        m_uLineCount++;
        scroll(m_iX, m_iY);
	}
    
    // indicate activity on LCD
    void blink()
    {
        if (millis() > m_uBlinkMillis)
        {
            m_serial.write(0xFE);
            m_serial.write(0x8F);
            delay(20);
    
            if (m_bBlink == true) m_serial.write('O');
            else m_serial.write('*');
            delay(10);
            
            m_bBlink = !m_bBlink;
            m_uBlinkMillis = millis() + 500;
        }
    }
    
    unsigned int width() const {return m_uBufWidth;}
    unsigned int height() const {return m_uBufHeightMask+1;}
    unsigned int x() const {return m_iX;}
    unsigned int y() const {return m_iY;}
    unsigned int lineCount() const {return m_uLineCount;}
    
  private:
    Stream             &m_serial;
    unsigned int       m_uBufWidth;
    unsigned int       m_uBufHeightMask;
    char               *m_pBuffer;
    unsigned long      m_uBufferSize;
    unsigned int       m_uLineCount;
    unsigned int       m_iX;
    unsigned int       m_iY;
    bool               m_bBlink;
    unsigned long      m_uBlinkMillis;
};




class LcdAnimator
{
  public:
    LcdAnimator(LcdScreen &_rLcd, int _iUpDownAnalogPin, int _iLeftRightAnalogPin)
        :m_rLcd(_rLcd),
         m_uLightOffTime(0),
         m_bBackLightOn(false),
         m_bBlink(false),
         m_iLeftRightAnalogPin(_iLeftRightAnalogPin),
         m_iUpDownAnalogPin(_iUpDownAnalogPin),
         m_bLeftRightDown(false),
         m_bUpDownDown(false)
    {
    }
    
    /// set blinking on/off
    void setBlink(bool _bState)
    {
        m_bBlink = _bState;
    }
    
    /// switches the backlight on until 'OffTime'
    void setBackLightOn(unsigned long _uSwitchOffTime)
    {
        m_uLightOffTime = _uSwitchOffTime;
        m_rLcd.setBacklight(20);
        m_bBackLightOn = true;
    }

    /// update LCD
    void update()
    {
        if (m_bBlink == true)
        {
            m_rLcd.blink();
        }
        
        if ( (m_bBackLightOn == true) &&
             (millis() > m_uLightOffTime) )
        {
            m_rLcd.setBacklight(0);
            m_bBackLightOn = false;
        }
        
        if (m_iUpDownAnalogPin > 0)
        {
            readUpDownButtons();
        }
        
        if (m_iLeftRightAnalogPin > 0)
        {
            readLeftRightButtons();
        }
    }

    
  protected:
    void readLeftRightButtons()
    {
        if (m_bLeftRightDown == false)
        {
            int i = analogRead(m_iLeftRightAnalogPin);
            if (i < 300)
            {
                m_bLeftRightDown = true;
                setBackLightOn(millis() + 4000);
                m_rLcd.scroll((int)m_rLcd.x() - 1, (int)m_rLcd.y());
            }
            else if (i > 700)
            {
                m_bLeftRightDown = true;
                setBackLightOn(millis() + 4000);
                m_rLcd.scroll((int)m_rLcd.x() + 1, (int)m_rLcd.y());
            }
        }
        else
        {
            int i = analogRead(m_iLeftRightAnalogPin);
            if ((i > 300) && (i < 700))
            {
                m_bLeftRightDown = false;
            }
        }
    }
    
    
    void readUpDownButtons()
    {
        if (m_bUpDownDown == false)
        {
            int i = analogRead(m_iUpDownAnalogPin);
            if (i < 300)
            {
                m_bUpDownDown = true;
                setBackLightOn(millis() + 4000);
                m_rLcd.scroll((int)m_rLcd.x(), (int)m_rLcd.y()-1);
            }
            else if (i > 700)
            {
                m_bUpDownDown = true;
                setBackLightOn(millis() + 4000);
                m_rLcd.scroll((int)m_rLcd.x(), (int)m_rLcd.y()+1);
            }
        }
        else
        {
            int i = analogRead(m_iUpDownAnalogPin);
            if ((i > 300) && (i < 700))
            {
                m_bUpDownDown = false;
            }
        }
    }
    
    
  protected:
    LcdScreen           &m_rLcd;
    unsigned long       m_uLightOffTime;
    bool                m_bBackLightOn;
    bool                m_bBlink;
    int                 m_iLeftRightAnalogPin;
    int                 m_iUpDownAnalogPin;
    bool                m_bLeftRightDown;
    bool                m_bUpDownDown;
};





#endif  // #ifndef LCD_H

