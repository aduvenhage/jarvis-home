
#include <Arduino.h>
#include <serialio.h>


class Gps
{
 public:
    Gps(Stream &_rSerial, int _iSleepPin)
        :m_serial(_rSerial),
         m_iSleepPin(_iSleepPin),
         m_bSleeping(false)
    {
        m_pszTime[0] = '\0';
        m_pszLatitude[0] = '\0';
        m_pszLongitude[0] = '\0';
        
        pinMode(m_iSleepPin, OUTPUT);
        digitalWrite(m_iSleepPin, HIGH);
        
        setup();
    }
    
    void setup()
    {
        m_serial.print("$PMTK220,1000*1F\r\n");         // set GPS update rate to 1000ms
        delay(200);
        m_serial.print("$PGCMD,16,1,0,0,0,0*6B\r\n");   // enable only RMC string
        delay(200);
    }
    
    /// sleep or wake-up GPS
    void sleep(bool _bState)
    {
        m_bSleeping = _bState;
        if (m_bSleeping == false)
        {
            digitalWrite(m_iSleepPin, HIGH);
        }
        else
        {
            digitalWrite(m_iSleepPin, LOW);
        }
    }
    
    /// read and process all data from GPS (returns true if new data was received)
    bool read()
    {
        bool bRx = false;
        int n = 0;
        while ((n = readln(m_serial, m_pszNmea, 255, 10)) > 0)
        {
            m_pszNmea[n] = '\0';
            
            // decode RMC string
            if (strncmp(m_pszNmea, "$GPRMC", 6) == 0)
            {
                strncpy(m_pszTime, m_pszNmea+7, 6);
                m_pszTime[6] = '\0';
                    
                m_pszLatitude[0] = m_pszNmea[30];
                strncpy(m_pszLatitude+1, m_pszNmea+20, 10);
                m_pszLatitude[10] = '\0';
                    
                m_pszLongitude[0] = m_pszNmea[43];
                strncpy(m_pszLongitude+1, m_pszNmea+32, 11);
                m_pszLongitude[11] = '\0';
                
                bRx = true;
            }
        }
        
        return bRx;
    }
    
    bool sleeping() const {return m_bSleeping;}
    const char *timeString() const {return m_pszTime;}
    const char *latitudeString() const {return m_pszLatitude;}
    const char *longitudeString() const {return m_pszLongitude;}
    
 private:
    Stream     &m_serial;
    int        m_iSleepPin;
    bool       m_bSleeping;
    
    char       m_pszNmea[256];
    char       m_pszTime[7];
    char       m_pszLatitude[11];
    char       m_pszLongitude[12];
};




