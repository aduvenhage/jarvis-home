#ifndef CELSHIELD_H
#define CELSHIELD_H
#include <Arduino.h>
#include "../serialio/serialio.h"
#include "../containers/containers.h"



#define DEBUG_PRINT(a) Serial.print(a);
#define DEBUG_PRINTLN(a) Serial.println(a);



class GprsSms
{
  protected:
	const static int	PROVIDER_TEXT_SIZE	= 32;
	const static int	SERVICE_TEXT_SIZE	= 96;
	const static int	MAX_SMS_SIZE		= 142;
	const static int	SCRATCH_SIZE		= 255;
	
  public:
	enum eGprsEvent
	{
		EGE_NONE = 0,
		EGE_NEW_MSG_RCV,
		EGE_SERVICE_TEXT_RCV,
        EGE_CALL_RCV,
	};
	
	struct sMessage
	{
		sMessage()
		{
			m_pszText[0] = '\0';
			m_pszNumber[0] = '\0';
		}
		
		sMessage(const char *_pszNumber)
		{
			m_pszText[0] = '\0';
			
			strncpy(m_pszNumber, _pszNumber, 12);
			m_pszNumber[12] = '\0';
		}
		
		sMessage(const char *_pszText, const char *_pszNumber)
		{
			strncpy(m_pszText, _pszText, MAX_SMS_SIZE);
			m_pszText[MAX_SMS_SIZE] = '\0';
			
			strncpy(m_pszNumber, _pszNumber, 12);
			m_pszNumber[12] = '\0';
		}
		
		char m_pszText[MAX_SMS_SIZE+1];
		char m_pszNumber[13];
	};
	
  public:
    GprsSms(Stream &_rStream, int _iPowerPin)
        :m_serial(_rStream),
         m_iPowerPin(_iPowerPin),
		 m_uTxBusyTime(0),
         m_iWaitFailCount(0),
         m_uLastTestTime(0)
    {
		m_pszServiceText[0] = '\0';
		m_pszProviderText[0] = '\0';
    }
    
    virtual ~GprsSms()
    {
    }
    
    bool busy()
    {
		// check for bad uBusyTime values
		if (m_uTxBusyTime > millis() + 30000ul)
		{
			m_uTxBusyTime = millis() + 5000ul;
            return false;
		}
        else
        {
            return m_uTxBusyTime > millis();
        }
    }
    
	/// wait for 'OK/ERROR' return from module
	int waitForReturn()
	{
		DEBUG_PRINT("wait - ");
		
		int n = readln(m_serial, m_pScratch, SCRATCH_SIZE, 5000, false);
        if (n == 0)
        {
            m_iWaitFailCount++;
            DEBUG_PRINTLN("");
            return 0;
        }
        else
        {
            m_iWaitFailCount = 0;
            
            m_pScratch[n] = '\0';
            DEBUG_PRINTLN(m_pScratch);
            
            delay(500);
            return n;
        }
	}

    /// service gprs tx and rx queues
    void update(unsigned long _uTimeOut)
    {
		// check for new data from grps
        read(_uTimeOut);
        
        // check if we should send
		if ( (busy() == false) &&
             (m_txMsgQueue.empty() == false) )
		{
            sendNextMessage();
		}
        
        // test and reset module
        if ( (busy() == false) &&
             (millis() - m_uLastTestTime > 15000ul) )
        {
			DEBUG_PRINTLN("update: testing...");
            
            m_serial.print("AT\r\n");
            waitForReturn();
            
            if (m_iWaitFailCount > 3)
            {
                DEBUG_PRINTLN("update: cycling power...");
                powerDown();
                powerUp();
            }
            
            m_uLastTestTime = millis();
        }
    }
	
	/// read all data from GPRS
	void read(unsigned long _uTimeOut)
	{
		// read from GPRS
		int n = 0;
		while ((n = readln(m_serial, m_pScratch, SCRATCH_SIZE, _uTimeOut, true)) > 0)
		{
			m_pScratch[n] = '\0';
			
			DEBUG_PRINT("read - ");
			DEBUG_PRINTLN(m_pScratch);
			
			// new message received
			if (strncmp(m_pScratch, "+CMTI:", 6) == 0)
			{
                DEBUG_PRINTLN("read - new messages received");
				m_rxEventQueue.push(EGE_NEW_MSG_RCV);
			}
			
			// 'checkAirtime()' text received
			else if (strncmp(m_pScratch, "+CUSD:", 6) == 0)
			{
				// tokenise return
				char *pszCmd = strtok(m_pScratch, "+: ");
				int mode = atoi(strtok(NULL, ", "));
				char *pszText = strtok(NULL, "\"");
				
				strncpy(m_pszServiceText, pszText, SERVICE_TEXT_SIZE);
				m_pszServiceText[SERVICE_TEXT_SIZE] = '\0';
				
                DEBUG_PRINT("read - service text received - ");
                DEBUG_PRINTLN(m_pszServiceText);
                
				m_rxEventQueue.push(EGE_SERVICE_TEXT_RCV);
				m_uTxBusyTime = millis();
			}
			
			// 'sendMessage()' reply received
			else if (strncmp(m_pScratch, "+CMGS:", 6) == 0)
			{
                DEBUG_PRINTLN("read - message send completed");
                
				waitForReturn();
				m_uTxBusyTime = millis();
			}
			
			// voice call received
			else if (strncmp(m_pScratch, "RING", 4) == 0)
			{
                DEBUG_PRINTLN("read - voice call received");
                
				m_rxEventQueue.push(EGE_CALL_RCV);
			}
		}
	}
	
    /// sends the next tx message in the queue
    bool sendNextMessage()
    {
        m_uTxBusyTime = millis() + 15000ul;
        
        if (m_txMsgQueue.empty() == false)
        {
            sMessage msg;
            m_txMsgQueue.pop(msg);
				
            // start message
            DEBUG_PRINT("sendMessage - ");
            DEBUG_PRINTLN(msg.m_pszNumber);
            
            m_serial.print("AT+CMGF=1\r\n");
		    waitForReturn();
            m_serial.print("AT+CMGS=\"");
            m_serial.print(msg.m_pszNumber);
            m_serial.print("\"\r\n");
		
            // wait for response
            DEBUG_PRINT("sendMessage wait - ");
            unsigned long t = millis() + 10000ul;
            while (t > millis())
            {
                if (m_serial.available() > 0)
                {
                    char ch = m_serial.read();
                    DEBUG_PRINT(ch);
                    if (ch == '>')
                    {
                        break;
                    }
                }
            }
		
            DEBUG_PRINTLN("");
            
            // send text
            DEBUG_PRINT("sendMessage - ");
            DEBUG_PRINTLN(msg.m_pszText);
            
            m_serial.print(msg.m_pszText);
            delay(100);
            m_serial.print("\r\n");
            delay(100);
            m_serial.write(0x1A);
            delay(100);
            m_serial.print("\r\n");
        }
    }
    
    /// lists all unread messages
    void readAllMessages()
    {
		DEBUG_PRINTLN("readAllMessages - request");
		
        m_serial.print("AT+CMGF=1\r\n");
		waitForReturn();
        m_serial.print("AT+CMGL=\"REC UNREAD\"\r\n");
		
		// read from GPRS
		int n = 0;
		while ((n = readln(m_serial, m_pScratch, SCRATCH_SIZE, 5000, false)) > 0)
		{
			m_pScratch[n] = '\0';
			
			// 'readAllMessages()' message list received
			DEBUG_PRINT("readAllMessages - ");
			DEBUG_PRINTLN(m_pScratch);
			
			if (strncmp(m_pScratch, "+CMGL:", 6) == 0)
			{
				// tokenise return
				char *pszCmd = strtok(m_pScratch, "+: ");
				int smsIndex = atoi(strtok(NULL, "\", "));
				char *pszStat = strtok(NULL, "\",");
				char *pszNumber = strtok(NULL, "\",");
				
				// read message text
				n = readln(m_serial, m_pScratch, SCRATCH_SIZE, 5000, false);
				m_pScratch[n] = '\0';
				
				// queue message
				m_rxMsgQueue.push(sMessage(m_pScratch, pszNumber));
			}
			else if (strcmp(m_pScratch, "OK") == 0)
			{
				break;
			}
		}
    }
    
    /// deletes the message at the given location
    void deleteMessage(int _iIndex)
    {
		DEBUG_PRINTLN("deleteMessage");
		
        m_serial.print("AT+CMGF=1\r\n");
		waitForReturn();
        m_serial.print("AT+CMGD=");
        m_serial.print(_iIndex);
        m_serial.print("\r\n");
		waitForReturn();
    }
    
    /// delete all read messages
    void deleteAllReadMessages()
    {
		DEBUG_PRINTLN("deleteAllReadMessages");
		
        m_serial.print("AT+CMGF=1\r\n");
		waitForReturn();
        m_serial.print("AT+CMGDA=\"DEL READ\"\r\n");
		waitForReturn();
    }
    
    /// delete all sent messages
    void deleteAllSentMessages()
    {
		DEBUG_PRINTLN("deleteAllSentMessages");
		
        m_serial.print("AT+CMGF=1\r\n");
		waitForReturn();
        m_serial.print("AT+CMGDA=\"DEL SENT\"\r\n");
		waitForReturn();
        m_serial.print("AT+CMGDA=\"DEL UNSENT\"\r\n");
		waitForReturn();
    }
    
    /// check provider
    void checkProvider()
    {
		DEBUG_PRINTLN("checkProvider");
		
		m_pszProviderText[0] = '\0';
        m_serial.print("AT+CMGF=1\r\n");
		waitForReturn();
        m_serial.print("AT+COPS?\r\n");
		
		// read from GPRS
		int n = 0;
		if ((n = readln(m_serial, m_pScratch, MAX_SMS_SIZE, 5000, false)) > 0)
		{
			m_pScratch[n] = '\0';
			
			DEBUG_PRINTLN(m_pScratch);
						
			// 'checkProvider()' text received
			if (strncmp(m_pScratch, "+COPS:", 6) == 0)
			{
				// tokenise return
				char *pszCmd = strtok(m_pScratch, "+: ");
				int mode = atoi(strtok(NULL, ", "));
				int format = atoi(strtok(NULL, ", "));
				char *pszText = strtok(NULL, "\",");
				strncpy(m_pszProviderText, pszText, PROVIDER_TEXT_SIZE);
                m_pszProviderText[PROVIDER_TEXT_SIZE] = '\0';
				
				waitForReturn();
			}
		}
    }
    
    /// check airtime
    void checkAirtime()
    {
		DEBUG_PRINTLN("checkAirtime");
		
        m_uTxBusyTime = millis() + 10000;
        
		m_pszServiceText[0] = '\0';
        m_serial.print("AT+CMGF=1\r\n");
		waitForReturn();
        m_serial.print("ATD*100#\r\n");
		waitForReturn();
    }
    
    /// switch GPRS module on
    void powerUp()
    {
		DEBUG_PRINTLN("powerUp");
		
        // flush module
        flush(m_serial);
        
        // try an AT command and switch on if there is no response
        m_serial.print("AT\r\n");
        if (waitForReturn() == 0)
        {
            pinMode(m_iPowerPin, OUTPUT);
            digitalWrite(m_iPowerPin,LOW);
            delay(1000);
            digitalWrite(m_iPowerPin,HIGH);
            delay(2000);
            digitalWrite(m_iPowerPin,LOW);
            delay(3000);
            
            // turn echo off
            m_serial.print("ATE0\r\n");
            
            // wait a little for GPRS to initialise
            delay(3000);
        }
        
        // flush module
        flush(m_serial);
    }
	
    /// switch GPRS module off
    void powerDown()
    {
		DEBUG_PRINTLN("powerDown");
		
        // flush module
        flush(m_serial);
        
        // try an AT command and switch on if there is a response
        m_serial.print("AT\r\n");
        if (waitForReturn() > 0)
        {
            pinMode(m_iPowerPin, OUTPUT);
            digitalWrite(m_iPowerPin,LOW);
            delay(1000);
            digitalWrite(m_iPowerPin,HIGH);
            delay(2000);
            digitalWrite(m_iPowerPin,LOW);
            delay(3000);
        }
        
        // flush module
        flush(m_serial);
    }
	
	/// returns the next event
	eGprsEvent popRxEvent()
	{
		char event = EGE_NONE;
		m_rxEventQueue.pop(event);
		return (eGprsEvent)event;
	}
	
	/// returns true if there are mesages in the queue
	bool hasRxMessages()
	{
		return m_rxMsgQueue.empty() == false;
	}
	
	/// pops the next message from the message queue
	sMessage &popRxMessage(sMessage &_rMsg)
	{
		m_rxMsgQueue.pop(_rMsg);
	}
	
    /// creates a message (using variable argument list) and queues it to be sent (message length is limited)
    void pushTxMessageFmt(const char *_pszPoneNo, const char *_pszFmt, ...)
    {
        // build message
		va_list ap;
		va_start(ap, _pszFmt);
        int n = vsnprintf(m_pScratch, SCRATCH_SIZE, _pszFmt, ap);
		va_end(ap);
        
        if (n > 0)
        {
            pushTxMessageTxt(_pszPoneNo, m_pScratch);
        }
    }
    
    /// creates a message (using just text) and queues it to be sent (can send very long messages)
    void pushTxMessageTxt(const char *_pszPoneNo, const char *_pszText)
    {
		DEBUG_PRINT("pushTxMessage - ");
		DEBUG_PRINTLN(_pszPoneNo);
		
        // queue message(s)
		DEBUG_PRINT("pushTxMessage - ");
		DEBUG_PRINTLN(_pszText);
        
        int n = strlen(_pszText);
        for (int i = 0; i < n; i += MAX_SMS_SIZE)
        {
            sMessage msg(_pszText+i, _pszPoneNo);
            
            DEBUG_PRINT(i);
            DEBUG_PRINT(": ");
            DEBUG_PRINTLN(msg.m_pszText);
            
            m_txMsgQueue.push(msg);
        }
    }
    
	const char *serviceText() const {return m_pszServiceText;}
	const char *providerText() const {return m_pszProviderText;}
    
  private:
    Stream					&m_serial;
    int						m_iPowerPin;
	char					m_pszServiceText[SERVICE_TEXT_SIZE+1];
	char					m_pszProviderText[PROVIDER_TEXT_SIZE+1];
    char                    m_pScratch[SCRATCH_SIZE+1];                 ///< buffer used internally
	unsigned long			m_uTxBusyTime;
	
	Queue<char, 8>          m_rxEventQueue;
	Queue<sMessage, 4>		m_txMsgQueue;
	Queue<sMessage, 4>		m_rxMsgQueue;
    
    int                     m_iWaitFailCount;
    unsigned long           m_uLastTestTime;
};





#endif  // #ifndef CELSHIELD_H

