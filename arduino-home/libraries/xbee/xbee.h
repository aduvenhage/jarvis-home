
#ifndef LIBXBEE_H
#define LIBXBEE_H


#include <Arduino.h>
#include "../serialio/serialio.h"


/** 
  Class for generating xbee AT commands.
  Use 'addCommand(...)' to add AT commands and the use 'executeCommands(...)' to send all the commands to the XBee.
*/
class XBeeCmd
{
  private:
    static const int BUF_SIZE = 128; ///< command buffer size (there is no checking for overflow when adding too many commands)
    
  public:
    XBeeCmd(Stream &_serial)
        :m_serial(_serial)
    {
        clearCommandBuffer();
    }
    
    virtual ~XBeeCmd()
    {
    }
    
    /// clear current command buffer
    void clearCommandBuffer()
    {
        m_pszCmdBuffer[0] = 'A';
        m_pszCmdBuffer[1] = 'T';
        m_pszCmdBuffer[2] = '\0';
    }

    /// add a single AT command
    void addCommand(const char *_pszCmd)
    {
        if (m_pszCmdBuffer[0] != '\0')
        {
            strcat(m_pszCmdBuffer, ",");
        }
        
        strcat(m_pszCmdBuffer, _pszCmd);
    }
    
    /// add a single at command with a string parameter
    void addCommand(const char *_pszCmd, const char *_pszParam)
    {
        addCommand(_pszCmd);
        strcat(m_pszCmdBuffer, _pszParam);
    }
    
    /// add a single at command with a integer parameter
    void addCommand(const char *_pszCmd, int _iParam)
    {
        addCommand(_pszCmd);
        itoa(_iParam, m_pszItoaBuf, 16);
        strcat(m_pszCmdBuffer, m_pszItoaBuf);
    }
    
    /// add a single at command with a integer parameter
    void addCommand(const char *_pszCmd, unsigned short _uParam)
    {
        addCommand(_pszCmd);
        itoa(_uParam, m_pszItoaBuf, 16);
        strcat(m_pszCmdBuffer, m_pszItoaBuf);
    }
    
    /// execute commands in command buffer and return true if successfull
    bool executeCommands()
    {
        // start
        enterAtCommandMode();
        
        // read response
        char buf[2];
        int n = read(buf, 2);
        if ( (n == 0) ||
             (buf[0] != 'O') )
        {
            return false;
        } 

        // write command        
        for (size_t i = 0; m_pszCmdBuffer[i]!='\0'; i++)
        {
            m_serial.write(m_pszCmdBuffer[i]);
            delay(50);
        }
        
        m_serial.write('\r');
        clearCommandBuffer();
        flush();
        
        return true;
    }
    
    /// return stream being used for XBee IO
    Stream &stream()
    {
        return m_serial;
    }
    
    // try to read until line is idle for 10ms or more
    void flush()
    {
        ::flush(m_serial);
    }
    
    // try to read until line is idle for 10ms or more, or until buffer is full
    unsigned int read(char *_pBuf, unsigned int _uBufSize)
    {
        return ::read(m_serial, _pBuf, _uBufSize);
    }

    // try to read until line is idle for _uTimeOutMs, until buffer is full, or until NL or CR is received; returns immediately if no bytes available and _bCheckAvailable == true
    unsigned int readln(char *_pBuf, unsigned int _uBufSize, unsigned long _uTimeOutMs, bool _bCheckAvailable)
    {
        return ::readln(m_serial, _pBuf, _uBufSize, _uTimeOutMs, _bCheckAvailable);
    }
    
    
  protected:
    void enterAtCommandMode()
    {
        // wait and flush to make sure rx buffer is clean before we program
        delay(1100);
        flush();
        
        m_serial.print("+");
        delay(100);
        m_serial.print("+");
        delay(100);
        m_serial.print("+");
        
        delay(1100);
    }
 
    
   private:
    Stream        &m_serial;
    char          m_pszCmdBuffer[BUF_SIZE];
    char          m_pszItoaBuf[8];
};


/**
  Utility class for FIO XBee radio.
  Will program XBee with the correct Arduino Fio settings when class is constructed (the status led is lit while programming).
  Serial port needs to be set at 57600 baud.
*/
class FioXBee
{
  public:
    /// the sleep pin has to be pulled down with a external resistor to keep it low while arduino is resetting (sleep is disabled if sleepPin < 0)
    FioXBee(Stream &_serial, unsigned long _uBaudRate, int _iSleepPin)
        :m_xBee(_serial),
         m_uBaudRate(_uBaudRate),
         m_iSleepPin(_iSleepPin),
         m_bSleeping(false)
    {
    }

    /// program xbee for Arduino/FIO program upload, IO, etc. 
    bool program(unsigned short _uMyAddr, unsigned short _uPanAddr)
    {
        // setup sleep pin
        if (m_iSleepPin >= 0)
        {
            pinMode(m_iSleepPin, OUTPUT);
            digitalWrite(m_iSleepPin, LOW);
        }
        
        // program xbee as fio radio
        m_xBee.addCommand("RE");
        m_xBee.addCommand("BD", getBaudRateCode(m_uBaudRate));  // desired baudrate
        m_xBee.addCommand("ID", _uPanAddr);   // PAN addr
        m_xBee.addCommand("MY", _uMyAddr);    // XBee addr
        m_xBee.addCommand("DL", "0");         // send to Prog/Central radio
        m_xBee.addCommand("D3", "5");
        m_xBee.addCommand("IC", "0");
        m_xBee.addCommand("RR", "0");    
        m_xBee.addCommand("IU", "0");
        m_xBee.addCommand("IA", "FFFF");
        m_xBee.addCommand("RO", "10");
        m_xBee.addCommand("SM", (m_iSleepPin >= 0) ? "2" : "0");          // pin sleep mode
        m_xBee.addCommand("SO", "3");          // disable on wake-up polling
        m_xBee.addCommand("WR");
        m_xBee.addCommand("CN");
        
        if (m_xBee.executeCommands() == false)
        {  
            return false;
        }

        // setup sleep pin
        if (m_iSleepPin >= 0)
        {
            digitalWrite(m_iSleepPin, m_bSleeping ? HIGH : LOW);
        }
        
        return true;  // success
    }
    
    /// program xbee for Arduino/Programmer program upload, IO, etc. 
    bool program(unsigned short _uPanAddr)
    {
        // setup sleep pin
        if (m_iSleepPin >= 0)
        {
            pinMode(m_iSleepPin, OUTPUT);
            digitalWrite(m_iSleepPin, LOW);
        }
        
        // program xbee as fio radio
        m_xBee.addCommand("RE");
        m_xBee.addCommand("BD", getBaudRateCode(m_uBaudRate));  // desired baudrate
        m_xBee.addCommand("ID", _uPanAddr);   // PAN addr
        m_xBee.addCommand("MY", "0");          // XBee addr
        m_xBee.addCommand("DL", "FFFF");       // broadcast to all radios
        m_xBee.addCommand("D3", "3");
        m_xBee.addCommand("IC", "8");
        m_xBee.addCommand("RR", "6");    
        m_xBee.addCommand("IU", "0");
        m_xBee.addCommand("IA", "0");
        m_xBee.addCommand("RO", "10");
        m_xBee.addCommand("SM", (m_iSleepPin >= 0) ? "2" : "0");          // pin sleep mode
        m_xBee.addCommand("WR");
        m_xBee.addCommand("CN");
        
        if (m_xBee.executeCommands() == false)
        {
            return false;   // failed
        }

        // setup sleep pin
        if (m_iSleepPin >= 0)
        {
            digitalWrite(m_iSleepPin, m_bSleeping ? HIGH : LOW);
        }
        
        return true;  // success
    }

    /// go into sleep mode by setting sleep output pin to HIGH (sleep pin in configured as output in 'init()' during construction)
    /// (the sleep pin has to be pulled down with a external resistor to keep it low while arduino is resetting)
    void sleep(bool _bState)
    {
        if ( (m_iSleepPin >= 0) &&
             (_bState != m_bSleeping) )
        {
            m_bSleeping = _bState;
            if (m_bSleeping == false) // coming out of sleep
            {
                digitalWrite(m_iSleepPin, LOW);
                delay(10);  // wait for warm-up
            }
            else // going to sleep
            {
                digitalWrite(m_iSleepPin, HIGH);
            }
        }
    }
     
    /// return stream being used for XBee IO
    Stream &stream()
    {
        return m_xBee.stream();
    }
        
    // try to read until line is idle for 100ms or more
    void flush()
    {   
        m_xBee.flush();
    }
        
    // try to read until line is idle for 10ms or more, or until buffer is full
    unsigned int read(char *_pBuf, unsigned int _uBufSize)
    {
        return m_xBee.read(_pBuf, _uBufSize);
    }
    
    // try to read until line is idle for _uTimeOutMs, until buffer is full, or until NL or CR is received; returns immediately if no bytes available and _bCheckAvailable == true
    unsigned int readln(char *_pBuf, unsigned int _uBufSize, unsigned long _uTimeOutMs, bool _bCheckAvailable)
    {
        return m_xBee.readln(_pBuf, _uBufSize, _uTimeOutMs, _bCheckAvailable);
    }
    
    unsigned long baudRate() const {return m_uBaudRate;}
    int sleepPin() const {return m_iSleepPin;}
    bool sleepEnabled() const {return m_iSleepPin >= 0;}
    bool sleeping() const {return m_bSleeping;}
    
  private:
    int getBaudRateCode(unsigned long _uBaudRate)
    {
        if (_uBaudRate == 1200) return 0;
        else if (_uBaudRate == 2400) return 1;
        else if (_uBaudRate == 4800) return 2;
        else if (_uBaudRate == 9600) return 3;
        else if (_uBaudRate == 19200) return 4;
        else if (_uBaudRate == 38400) return 5;
        else if (_uBaudRate == 57600) return 6;
        else if (_uBaudRate == 115200) return 7;
        else return 6;
    }  
  
  private:
    XBeeCmd        m_xBee;
    unsigned long  m_uBaudRate;
    int            m_iSleepPin;     ///< sleep pin is not used (sleep disabled) if m_iSleepPin < 0
    bool           m_bSleeping;
};





#endif  // #ifndef LIBXBEE_H
