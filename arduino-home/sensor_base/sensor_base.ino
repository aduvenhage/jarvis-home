
 
#include <stdio.h>
#include <EEPROM.h>
#include <xbee.h>
#include <blink.h>
#include <celshield.h>
#include <lcd.h>
#include <deviceconfig.h>
#include <containers.h>
#include <taskmanager.h>
#include "phone_numbers.h"          // defines PHONE_NO_DEFAULT


// constants
#define              ALARM_OUTPUT_PIN              4
#define              GPRS_POWER_PIN                9
#define              DEVICE_STATUS_LED_PIN         13

#define              VIN_PIN                       A0
#define              LEFTRIGHT_PIN                 A1
#define              UPDOWN_PIN                    A2

#define              RADIO_PAN_ID                  0x1235            ///< radio network id
#define              RADIO_BAUD                    57600             ///< radio operating baud
#define              GPRS_BAUD                     19200             ///< gprs shield operating baud

#define              LCD_SERIAL                    Serial1
#define              GPRS_SERIAL                   Serial2
#define              RADIO_SERIAL                  Serial3

#define              MAX_SENSORS                   16
#define              SMS_WAIT_TIME                 1000ul            ///< [ms] time to wait between event sms calls
#define              SENSOR_TIMEOUT                1000ul*600ul      ///< [ms] maximum time allowed between sensor status updates
#define              MIN_SENSOR_VB                 360               ///< [V*100] minimum safe voltage for sensor batteries  


// voltage constants
#define              DEVICE_VCC                    5.0f                     ///< [V] Mega supply voltage
const float          VIN_DEVIDER                   = 1.0f / 3.0f;
const float          VIN_RESOLUTION                = DEVICE_VCC / 1023.0f;


// variables
FioXBee                       *gRadio = NULL;
LcdScreen                     *gLcd = NULL;
LcdAnimator                   *gLcdAnimator = NULL;
GprsSms                       *gGprs = NULL;
TaskManager                   gTaskManager;

Queue<sDeviceData, 8>         gDeviceDataQueue;                     ///< sensor status updates are queued until relevant processing task is run

sDeviceData                   gDeviceData[MAX_SENSORS];             ///< keeps last update from all sensors (sensor addr-1 is used as the index)
unsigned long                 gAlarmOffTime = 0;                    ///< time when alarm will be switched off, set when sensor event occurs
unsigned short                gRxCounter = 0;                       ///< counts the number of messages received from sensors

int                           gSensorPriorityLevel = 0x03;          ///< includes all sensor events with a priority <= gEventPriorityLevel; -1 ignores all sensor events
bool                          gSensorSirenOn = true;                ///< siren will sound if events with a priority <= gEventPriorityLevel occurs
bool                          gSensorLowVbSmsOn = true;             ///< sms sensor low voltage events if set to true
bool                          gSensorTimeoutSmsOn = true;           ///< sms sensor timeout events if set to true
bool                          gPowerFailureSmsOn = true;            ///< sms power failure events if set to true

char                          gszPhoneNo[PHONE_NO_MAX_CHARS+1] = PHONE_NO_DEFAULT;   ///< phone number used for event sms's





/// reads and returns VIN
float inputVoltage()
{
    return (float)analogRead(VIN_PIN) * VIN_RESOLUTION / VIN_DEVIDER;
}


/// reads VIN and returns a smoothed VIN
float aveInputVoltage()
{
    static float gVinAve = inputVoltage();
    
    float gVin = inputVoltage();
    gVinAve = gVin * 0.001 + gVinAve * 0.999;
    
    return gVinAve;
}


/// read and process all data from radio
void readFromRadio()
{
    char pszRadioRx[48];
    unsigned int n = 0;
    while ((n = gRadio->readln(pszRadioRx, 47, 50, true)) > 0)
    {
        pszRadioRx[n] = '\0';
        Serial.println(pszRadioRx);
        
        sDeviceData data;
        decodeDeviceData(data, pszRadioRx);
        if ( (data._uAddr > 0) && (data._uAddr < MAX_SENSORS) &&
             (data._uPriority > 0) && (data._uPriority < 16) )
        {
            data._uTimestamp = millis();
            gDeviceDataQueue.push(data);
            
            gRxCounter++;
            if (gRxCounter > 9999)
            {
                gRxCounter = 0;
            }
        }
    }
}


/// Gprs read task
void readGprsQuick()
{
    gGprs->update(500);
}


/// builds text string for given device data structure
void buildSensorString(char *_pszBuf, size_t _uBufSize, sDeviceData &_data)
{
    if (_data._uAddr > 0)
    {
        if (_data._uTimestamp > 0)
        {
            unsigned long dt = (millis() - _data._uTimestamp) / 1000ul;
            
            if (_data._uTemperature > 0)
            {
                snprintf(_pszBuf, _uBufSize, "%s,%dVb,%dC,%us\n", _data._pszName, _data._uBtyVoltage, _data._uTemperature, dt);
            }
            else
            {
                snprintf(_pszBuf, _uBufSize, "%s,%dVb,%us\n", _data._pszName, _data._uBtyVoltage, dt);
            }
        }
        else
        {
            if (_data._uTemperature > 0)
            {
                snprintf(_pszBuf, _uBufSize, "%s,%dVb,%dC,-\n", _data._pszName, _data._uBtyVoltage, _data._uTemperature);
            }
            else
            {
                snprintf(_pszBuf, _uBufSize, "%s,%dVb,-\n", _data._pszName, _data._uBtyVoltage);
            }
        }
    }
    else
    {
        snprintf(_pszBuf, _uBufSize, "");
    }
}


/// collect sensor data and sms it
void smsSensorData(const char *_pszMsgNo)
{
    char buf[32];
    char text[MAX_SENSORS * 32];
    text[0] = '\0';

    // build text for all sensors    
    for (size_t i = 0; i < MAX_SENSORS; i++)
    {
        buildSensorString(buf, 32, gDeviceData[i]);
        strcat(text, buf);
    }
        
    // send message
    gGprs->pushTxMessageTxt(_pszMsgNo, text);
}


/// sms status to given number 
void smsStatus(const char *_pszMsgNo)
{
    gGprs->pushTxMessageFmt(_pszMsgNo, 
                            "Vin %u\nfree ram %d\n%s\nx %u\nSIREN %s\nTIMEOUT %s\nLOWVB %s\nPOWER %s", 
                            (unsigned short)(inputVoltage()*100 + 0.5f),
                            freeRam(),
                            gszPhoneNo,
                            gSensorPriorityLevel,
                            gSensorSirenOn == true ? "ON" : "OFF",
                            gSensorTimeoutSmsOn == true ? "ON" : "OFF",
                            gSensorLowVbSmsOn == true ? "ON" : "OFF",
                            gPowerFailureSmsOn == true ? "ON" : "OFF");
}


/// process messages and events from GPRS module
void processGprsEvents()
{
    // read text messages
    if (gGprs->hasRxMessages() == true)
    {
        GprsSms::sMessage msg;
        
        // take message from gprs module
        gGprs->popRxMessage(msg);
        covertToUpper(msg.m_pszText);

        // print message to LCD
        gLcd->writeLine("%s %s", msg.m_pszText, msg.m_pszNumber);
        
        // process message
        if (strcmp(msg.m_pszText, "STATUS") == 0)
        {
            smsStatus(msg.m_pszNumber);
        }
        else if (strcmp(msg.m_pszText, "AIRTIME") == 0)
        {
            gGprs->checkAirtime();        
        }
        else if (strcmp(msg.m_pszText, "RESET") == 0)
        {
            gGprs->powerDown();
            reset();        
        }
        else if (strcmp(msg.m_pszText, "GPRS OFF") == 0)
        {
            gGprs->powerDown();
        }
        else if (strcmp(msg.m_pszText, "SENSORS") == 0)
        {
            smsSensorData(msg.m_pszNumber);
        }
        else if (strncmp(msg.m_pszText, "SET", 3) == 0)
        {
            // process command
            int iValue = atoi(msg.m_pszText + 3);
            gSensorPriorityLevel = iValue;

            // confirm command
            gGprs->pushTxMessageFmt(msg.m_pszNumber, "SET %d", gSensorPriorityLevel);
            if (strcmp(gszPhoneNo, msg.m_pszNumber) != 0)
            {
                gGprs->pushTxMessageFmt(gszPhoneNo, "SET %d", gSensorPriorityLevel);
            }
        }
        else if (strncmp(msg.m_pszText, "SIREN", 5) == 0)
        {
            gSensorSirenOn = strncmp(msg.m_pszText + 6, "ON", 2) == 0;

            // confirm command
            gGprs->pushTxMessageFmt(msg.m_pszNumber, "SIREN %s", gSensorSirenOn == true ? "ON" : "OFF");
            if (strcmp(gszPhoneNo, msg.m_pszNumber) != 0)
            {
                gGprs->pushTxMessageFmt(gszPhoneNo, "SIREN %s", gSensorSirenOn == true ? "ON" : "OFF");
            }
        }
        else if (strncmp(msg.m_pszText, "LOWVB", 5) == 0)
        {
            gSensorLowVbSmsOn = strncmp(msg.m_pszText + 6, "ON", 2) == 0;

            // confirm command
            gGprs->pushTxMessageFmt(msg.m_pszNumber, "LOWVB %s", gSensorLowVbSmsOn == true ? "ON" : "OFF");
            if (strcmp(gszPhoneNo, msg.m_pszNumber) != 0)
            {
                gGprs->pushTxMessageFmt(gszPhoneNo, "LOWVB %s", gSensorLowVbSmsOn == true ? "ON" : "OFF");
            }
        }
        else if (strncmp(msg.m_pszText, "POWER", 5) == 0)
        {
            gPowerFailureSmsOn = strncmp(msg.m_pszText + 6, "ON", 2) == 0;

            // confirm command
            gGprs->pushTxMessageFmt(msg.m_pszNumber, "POWER %s", gPowerFailureSmsOn == true ? "ON" : "OFF");
            if (strcmp(gszPhoneNo, msg.m_pszNumber) != 0)
            {
                gGprs->pushTxMessageFmt(gszPhoneNo, "POWER %s", gPowerFailureSmsOn == true ? "ON" : "OFF");
            }
        }
        else if (strncmp(msg.m_pszText, "TIMEOUT", 7) == 0)
        {
            gSensorTimeoutSmsOn = strncmp(msg.m_pszText + 8, "ON", 2) == 0;

            // confirm command
            gGprs->pushTxMessageFmt(msg.m_pszNumber, "TIMEOUT %s", gSensorTimeoutSmsOn == true ? "ON" : "OFF");
            if (strcmp(gszPhoneNo, msg.m_pszNumber) != 0)
            {
                gGprs->pushTxMessageFmt(gszPhoneNo, "TIMEOUT %s", gSensorTimeoutSmsOn == true ? "ON" : "OFF");
            }
        }
        else if (strncmp(msg.m_pszText, "PHONESET", 8) == 0)
        {
            strncpy(gszPhoneNo, msg.m_pszNumber, PHONE_NO_MAX_CHARS);
            gszPhoneNo[PHONE_NO_MAX_CHARS] = '\0';

            // store new number in EEPROM
            for (char i = 0; i < PHONE_NO_MAX_CHARS; i++)
            {
                if (gszPhoneNo[i] == '\0')
                {
                    for (; i < PHONE_NO_MAX_CHARS; i++)
                    {
                        EEPROM.write(i, '\0');
                    }
                    
                    break;
                }
                else
                {                
                    EEPROM.write(i, gszPhoneNo[i]);
                }
            }
            
            // confirm command
            gGprs->pushTxMessageFmt(msg.m_pszNumber, "PHONESET %s", gszPhoneNo);
        }
        else if (strncmp(msg.m_pszText, "PHONE", 5) == 0)
        {
            // confirm command
            gGprs->pushTxMessageFmt(msg.m_pszNumber, "PHONE %s", gszPhoneNo);
        }
    }
    
    // read events if there are no text messages
    else
    {
        GprsSms::eGprsEvent evt;
        evt = gGprs->popRxEvent();
        
        if (evt == GprsSms::EGE_NEW_MSG_RCV)
        {
            gGprs->readAllMessages();
            gGprs->deleteAllReadMessages();
        }
        else if (evt == GprsSms::EGE_SERVICE_TEXT_RCV)
        {
            gLcd->writeLine(gGprs->serviceText());
            
            // send back SMS with service text, but only after 60s to prevent SMSs on startup 
            if (millis() > 60000)
            {
                gGprs->pushTxMessageTxt(gszPhoneNo, gGprs->serviceText());
            }
        }
        else if (evt == GprsSms::EGE_CALL_RCV)
        {
            gLcd->writeLine("call event received. resetting...");
            gGprs->powerDown();
            reset();
        }
    }
}


/// process and report sensor data from data queue
void processSensorUpdates()
{
    if (gDeviceDataQueue.empty() == false)
    {              
        sDeviceData data;
        gDeviceDataQueue.pop(data);
        sDeviceData &oldData = gDeviceData[data._uAddr-1];
        
        // check that data is new
        // - data may be sent multiple times from sensors, also assumes an eventCount of zero is invalid
        // - ignores data if events come through too quickly 
        bool bAllowUpdate = (data._uEventCount > 0) && (data._uEventCount != oldData._uEventCount);
        bAllowUpdate &= (data._uTimestamp - oldData._uTimestamp > 10000) || (data._bD2Event != oldData._bD2Event) || (data._bD3Event != oldData._bD3Event);
        
        if (bAllowUpdate)
        {
            // store data
            gDeviceData[data._uAddr-1] = data;
            
            // create data string
            char rxBuf[48];
            encodeDeviceData(rxBuf, 48, data);
            
            // process sensor events and status updates seperately
            if ( (data._bD2Event == true) || (data._bD3Event == true) )
            {
                // sensor event
                gLcdAnimator->setBackLightOn(millis() + 10000);
                gLcd->writeLine("evt %4u %s", gRxCounter, rxBuf);
                
                // check priority and flag alarm
                if (data._uPriority <= gSensorPriorityLevel)
                { 
                    // sound alarm
                    if (gSensorSirenOn == true)
                    {
                        gAlarmOffTime = millis() + 1000;
                        digitalWrite(ALARM_OUTPUT_PIN, HIGH);
                    }
                    
                    // send sms
                    gGprs->pushTxMessageFmt(gszPhoneNo, "evt %4u %s", gRxCounter, rxBuf);
                }
            }
            else
            {   
                // status update
                gLcd->writeLine("%4u %s", gRxCounter, rxBuf);
            }
        }
    }                    
}


/// check for sensor problems
void checkSensorStatus()
{
    for (size_t i = 0; i < MAX_SENSORS; i++)
    {
        sDeviceData &data = gDeviceData[i];
        if ( (data._uAddr == i + 1) &&
             (data._uTimestamp > 0) )   // only check if time attribute is valid
        {
            if (data._uBtyVoltage < MIN_SENSOR_VB)
            {
                data._uTimestamp = 0;  // reset time attribute
                gLcd->writeLine("btylow: %s,%dVb", data._pszName, data._uBtyVoltage);
                
                if (gSensorLowVbSmsOn == true)
                {
                    gGprs->pushTxMessageFmt(gszPhoneNo, "btylow: %s,%dVb", data._pszName, data._uBtyVoltage);
                }
            }
            else if (millis() - data._uTimestamp > SENSOR_TIMEOUT)
            {
                data._uTimestamp = 0;  // reset time attribute
                gLcd->writeLine("timeout: %s,%dVb", data._pszName, data._uBtyVoltage);
                
                if (gSensorTimeoutSmsOn == true)
                {
                    gGprs->pushTxMessageFmt(gszPhoneNo, "timeout: %s,%dVb", data._pszName, data._uBtyVoltage);
                }
            }
        }
    }
}


/// animate LCD, status LED and alarm status
void animate()
{
    gLcdAnimator->update();
    flash(2000, DEVICE_STATUS_LED_PIN);
    
    if (millis() > gAlarmOffTime)
    {
       digitalWrite(ALARM_OUTPUT_PIN, LOW);  
    }
}


/// check supply voltage for power outs, etc.
void checkSupplyVoltage()
{
    static unsigned long        gVinWaitTime = 0;
    
    if (millis() > gVinWaitTime)
    {
        static bool    gVinHigh = true;
        static float   gVinRef = inputVoltage();
        float          fVinAve = aveInputVoltage();
        
        Serial.print(fVinAve);
        Serial.print(" ");
        Serial.println(gVinRef);
        
        if (gVinHigh == true)
        {
            // set ref at max Vin, but also slowly recover from peaks
            gVinRef = max(gVinRef, fVinAve) * 0.999 + fVinAve * 0.001;
            
            // check if Vin is falling
            if (gVinRef - fVinAve > 0.05)
            {
                gVinHigh = false;
                gVinRef = fVinAve;
                
                gLcd->writeLine("Power supply is off");
                if (gPowerFailureSmsOn == true)
                {
                    gGprs->pushTxMessageTxt(gszPhoneNo, "Power supply is off");
                }
            }
        }
        else
        {
            // set ref at mimimum Vin
            gVinRef = min(gVinRef, fVinAve);
            
            // check if Vin is rising
            if (fVinAve - gVinRef > 0.1)
            {
                gVinHigh = true;
                gVinRef = fVinAve;
                
                gLcd->writeLine("Power supply is on");                
                if (gPowerFailureSmsOn == true)
                {
                    gGprs->pushTxMessageTxt(gszPhoneNo, "Power supply is on");
                }
            }
        }
        
        gVinWaitTime = millis() + 100;
    }
}


/// arduino port and pin setup
void setup()
{
    // startup/programming delay before pins and interrupts are changed
    waitAndFlash(4000, 500, DEVICE_STATUS_LED_PIN);

    // set Mega pins 0 to 53 as inputs with pullups (leaves RX and TX pins alone)
    for (unsigned char i = 2; i <= 13; i++) {pinMode(i, INPUT_PULLUP);}
    for (unsigned char i = 20; i <= 53; i++) {pinMode(i, INPUT_PULLUP);}
    
    // setup alarm output pin
    pinMode(ALARM_OUTPUT_PIN, OUTPUT);
    digitalWrite(ALARM_OUTPUT_PIN, LOW);

    // setup status LED output pin    
    pinMode(DEVICE_STATUS_LED_PIN, OUTPUT);
    digitalWrite(DEVICE_STATUS_LED_PIN, LOW);
  
    // setup GPRS module power pin
    pinMode(GPRS_POWER_PIN, OUTPUT);
    digitalWrite(GPRS_POWER_PIN, LOW);  
    
    // start PC debug port
    Serial.begin(115200);
    
    // setup radio module
    RADIO_SERIAL.begin(RADIO_BAUD);
    gRadio = new FioXBee(RADIO_SERIAL, RADIO_BAUD, -1);
    
    // setup GPRS module
    GPRS_SERIAL.begin(19200);
    gGprs = new GprsSms(GPRS_SERIAL, GPRS_POWER_PIN);
    
    // setup LCD module
    LCD_SERIAL.begin(9600);
    gLcd = new LcdScreen(LCD_SERIAL, 48, 32);
    gLcdAnimator = new LcdAnimator(*gLcd, UPDOWN_PIN, LEFTRIGHT_PIN);
    gLcdAnimator->setBlink(true);
    
    // write startup messages to LCD
    gLcdAnimator->setBackLightOn(millis() + 20000);        
    gLcd->writeLine("startup..");
        
    gLcd->writeLine("- input voltage: %u", (unsigned short)(inputVoltage()*100 + 0.5f));
    gLcd->writeLine("- LCD size: %dx%d", gLcd->width(), gLcd->height());
    gLcd->writeLine("- radio data queue size: %d", (int)gDeviceDataQueue.size());
      
    // program radio device
    gLcd->writeLine("radio start..");
    if (gRadio->program(RADIO_PAN_ID) == false)
    {
        gLcd->writeLine("radio failed, retrying with default baud..");
        waitAndFlash(1000, 200, DEVICE_STATUS_LED_PIN);
        
        // retry with default radio baud rate if programming failed
        RADIO_SERIAL.begin(9600);
        if (gRadio->program(RADIO_PAN_ID) == false)
        {
            gLcd->writeLine("radio error!");
        }
        else
        {
            gLcd->writeLine("radio OK");
        }
        
        // reset baud rate
        RADIO_SERIAL.begin(RADIO_BAUD);
    }
    else
    {
        gLcd->writeLine("radio OK");
    }
        
    // setup GPRS shield
    gLcd->writeLine("GPRS start..");
    gGprs->powerUp();
    gGprs->deleteAllReadMessages();
    gGprs->deleteAllSentMessages();
    gGprs->checkProvider();
    gLcd->writeLine(gGprs->providerText());
    gLcd->writeLine("GPRS OK");
    gLcd->writeLine("phone no: %s", gszPhoneNo);
    
    // read phone no from EEPROM
    if (EEPROM.read(0) == '+')
    {
        for (char i = 0; i < PHONE_NO_MAX_CHARS; i++)
        {
            gszPhoneNo[i] = EEPROM.read(i);
        }
    }
    
    // add base station tasks
    gTaskManager.addTask(readFromRadio, TaskManager::ETP_HIGH);
    gTaskManager.addTask(processSensorUpdates, TaskManager::ETP_NORMAL);
    gTaskManager.addTask(readGprsQuick, TaskManager::ETP_NORMAL);
    gTaskManager.addTask(animate, TaskManager::ETP_LOW);
    gTaskManager.addTask(processGprsEvents, TaskManager::ETP_LOW);
    gTaskManager.addTask(checkSupplyVoltage, TaskManager::ETP_LOW);
    gTaskManager.addTask(checkSensorStatus, TaskManager::ETP_LOW);
            
    // check available RAM    
    gLcd->writeLine("free ram: %d", freeRam());
    gLcd->writeLine("ready");
    gLcd->writeLine("");
    
    // sms alarm status after reset
    smsStatus(gszPhoneNo);
}


void loop()
{
    gTaskManager.run();
}

