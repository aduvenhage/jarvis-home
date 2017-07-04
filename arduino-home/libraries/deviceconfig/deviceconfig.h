
#ifndef LIBCONFIG_H
#define LIBCONFIG_H


#include <Arduino.h>
#include <EEPROM.h>
#include "../serialio/serialio.h"



struct sDeviceData
{
	sDeviceData()
        :_uPriority(0),
         _uAddr(0),
         _uBtyVoltage(0),
         _uChgVoltage(0),
         _uTemperature(0),
         _uEventCount(0xFFFF),
         _bTimeEvent(false),
         _bD2Event(false),
         _bD3Event(false),
         _uTimestamp(0)
	{
        memset(_pszName, '\0', 10);
	}
	
	sDeviceData(const sDeviceData &_rValue)
        :_uPriority(_rValue._uPriority),
         _uAddr(_rValue._uAddr),
         _uBtyVoltage(_rValue._uBtyVoltage),
         _uChgVoltage(_rValue._uChgVoltage),
         _uTemperature(_rValue._uTemperature),
         _uEventCount(_rValue._uEventCount),
         _bTimeEvent(_rValue._bTimeEvent),
         _bD2Event(_rValue._bD2Event),
         _bD3Event(_rValue._bD3Event),
         _uTimestamp(_rValue._uTimestamp)
    {
        strncpy(_pszName, _rValue._pszName, 10);
        _pszName[9] = '\0';
    }
    
    unsigned char     _uPriority;       ///< priority of sensor (lower values are higher priority, with 0 being the highest)
    char              _pszName[10];      ///< sensor description
    unsigned short    _uAddr;           ///< radio address
    
    unsigned short    _uBtyVoltage;
    unsigned short    _uChgVoltage;
    unsigned char     _uTemperature;
    
    unsigned short    _uEventCount;     ///< sensor event/update count
    
    bool              _bTimeEvent;
    bool              _bD2Event;
    bool              _bD3Event;
    
    unsigned long     _uTimestamp;
};



/// creates a string from device data
char *encodeDeviceData(char *_pszMessage, unsigned char _uSize, const sDeviceData &_rData)
{
    _pszMessage[0] = '\0';
    snprintf(_pszMessage, _uSize,
             "%s,%u,%ux,%uVb,%uVc,%uC,%ue,%u,%u,%u",
             _rData._pszName,
             _rData._uAddr,
             _rData._uPriority,
             _rData._uBtyVoltage,
             _rData._uChgVoltage,
             _rData._uTemperature,
             _rData._uEventCount,
             _rData._bTimeEvent ? 1 : 0,
             _rData._bD2Event ? 1 : 0,
             _rData._bD3Event ? 1 : 0);
    
    return _pszMessage;
}


/// creates a device data from a string
sDeviceData &decodeDeviceData(sDeviceData &_rData, char *_pszMessage)
{
    strncpy(_rData._pszName, strtok(_pszMessage, ","), 10);
	_rData._pszName[9] = '\0';
	
    _rData._uAddr = atoi(strtok(NULL, ","));
    _rData._uPriority = atoi(strtok(NULL, ","));
    _rData._uBtyVoltage = atoi(strtok(NULL, ","));
    _rData._uChgVoltage = atoi(strtok(NULL, ","));
    _rData._uTemperature = atoi(strtok(NULL, ","));
    _rData._uEventCount = atoi(strtok(NULL, ","));
    _rData._bTimeEvent = atoi(strtok(NULL, ",")) == 1;
    _rData._bD2Event = atoi(strtok(NULL, ",")) == 1;
    _rData._bD3Event = atoi(strtok(NULL, ",")) == 1;
    
    return _rData;
}


/// device config reading and storing
class DeviceConfig
{
 public:
    DeviceConfig(Stream &_rSerial, int _iStatusPin)
        :m_serial(_rSerial),
         m_iStatusPin(_iStatusPin)
    {
        memset(&m_config, 0, sizeof(sDeviceData));
    }
    
    
    /// save config to EEPROM
    void storeToEeprom()
    {
        const char *pDn = (const char*)&m_config;
        for (size_t i = 0; i < sizeof(sDeviceData); i++)
        {
            EEPROM.write(i,*pDn);
            pDn++;
        }
    }
    
    
    /// load config from EEPROM
    void loadFromEeprom()
    {
        char *pDn = (char*)&m_config;
        for (size_t i = 0; i < sizeof(sDeviceData); i++)
        {
            *pDn = EEPROM.read(i);
            pDn++;
        }
        
        // set some defaults if EEPROM is blank
        if (m_config._uAddr == 0xFFFF)
        {
            m_config._uAddr = 1;
            m_config._uPriority = 0;
            m_config._pszName[0] = '\0';
        }
    }
    
    
    /// process AT-like commands and config device, returns true if config changed
    bool processConfigCommand(const char *_pszCmd)
    {
        bool bConfigChanged = false;
        
        // process 'device test' command
        if (strncmp(_pszCmd, "ATDT", 4) == 0)
        {
            m_serial.println("TEST/ECHO mode");
            
            while (1)
            {
                if (m_serial.available() > 0)
                {
                    int ch = m_serial.read();
                    m_serial.write(ch);
                    waitOnOff(50, m_iStatusPin);
                }
            }
        }
        else if (strncmp(_pszCmd, "ATDN", 4) == 0)
        {
            if (strlen(_pszCmd) > 4)
            {
                strncpy(m_config._pszName, _pszCmd + 4, 10);
                m_config._pszName[9] = '\0';
                m_serial.println("OK");
                bConfigChanged = true;
            }
            else
            {
                m_serial.println(m_config._pszName);
            }
        }
        else if (strncmp(_pszCmd, "ATDA", 4) == 0)
        {
            if (strlen(_pszCmd) > 4)
            {
                m_config._uAddr = (unsigned short)atoi(_pszCmd + 4);
                m_serial.println("OK");
                bConfigChanged = true;
            }
            else
            {
                m_serial.println(m_config._uAddr);
            }
        }
        else if (strncmp(_pszCmd, "ATDX", 4) == 0)
        {
            if (strlen(_pszCmd) > 4)
            {
                m_config._uPriority = (unsigned short)atoi(_pszCmd + 4);
                m_serial.println("OK");
                bConfigChanged = true;
            }
            else
            {
                m_serial.println(m_config._uPriority);
            }
        }
        else
        {
            m_serial.println("ERR");
        }
        
        return bConfigChanged;
    }
    
    
    /// wait for, read and process config commands, returns true if config changed
    bool waitForConfig(unsigned long _uWaitTime)
    {
        bool bConfigChanged = false;
        digitalWrite(m_iStatusPin, HIGH);
        
        // read command lines until a empty line is received
        char pszCmdBuffer[256];
        unsigned int n = ::readln(m_serial, pszCmdBuffer, 255, _uWaitTime, false);
        while (n > 0)
        {
            pszCmdBuffer[n] = '\0';
            bConfigChanged |= processConfigCommand(pszCmdBuffer);
            
            n = ::readln(m_serial, pszCmdBuffer, 255, _uWaitTime, false);
        }
        
        // flush all remaining rx data from radio
        digitalWrite(m_iStatusPin, LOW);
        ::flush(m_serial);
        return bConfigChanged;
    }
    
    
    sDeviceData &config() {return m_config;}
    const sDeviceData &config() const {return m_config;}
    
 private:
    sDeviceData             m_config;
    Stream                  &m_serial;
    int                     m_iStatusPin;
};



#endif //#ifndef LIBCONFIG_H

