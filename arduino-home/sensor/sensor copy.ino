#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <EEPROM.h>
#include <xbee.h>
#include <blink.h>
#include <deviceconfig.h>



// constants
#define              EVTD2_INT_NO                  0
#define              EVTD3_INT_NO                  1

#define              EVTD2_INT_PIN                 2
#define              EVTD3_INT_PIN                 3
#define              RADIO_SLEEP_PIN               12
#define              DEVICE_STATUS_LED_PIN         13

#define              BTY_PIN                       A0
#define              CHG_PIN                       A1
#define              TMP_PIN                       A2

#define              RADIO_PAN_ID                  0x1235            ///< radio network id
#define              RADIO_BAUD                    57600             ///< radio operating baud
#define              OUTPUT_BUF_SIZE               64


// voltage constants
#define              DEVICE_VCC                    3.3f             ///< [V] Fio supply voltage
const float          BTY_DEVIDER                   = 10.0f / (10.0f + 10.0f);
const float          BTY_RESOLUTION                = DEVICE_VCC / 1023.0f;
const float          CHG_DEVIDER                   = 10.0f / (10.0f + 10.0f);
const float          CHG_RESOLUTION                = DEVICE_VCC / 1023.0f;
const float          TMP_RESOLUTION                = 100.0f * DEVICE_VCC / 1023.0f;


// timer constants
#define              DEVICE_CLOCK_HZ               F_CPU             ///< [Hz]
#define              TMR_DESIRED_TIMEOUT_S         240               ///< DESIRED_TIMEOUT is the desired time period (in seconds) between timer events

const float          TMR_OVERFLOW_S               = 8.0;             ///< should match WDT timeout (max 8s)
const unsigned long  TMR_OVERFLOW_COUNT           = (unsigned long)((float)TMR_DESIRED_TIMEOUT_S / (float)TMR_OVERFLOW_S + 0.5f);


// variables
volatile bool          gSensorEventD2 = false;            /// set by ISR when D2 goes high
volatile bool          gSensorEventD3 = false;            /// set by ISR when D3 goes high
volatile bool          gTimeEvent = false;                /// set by ISR when timer count reaches TMR1_OVERFLOW_COUNT 
volatile bool          gSensorEventsEnabled = false;      /// sensor events are ignored when flag is false
volatile unsigned long gTimeEventCounter = 0;             /// current timer ISR count

FioXBee                *gRadio = NULL;
DeviceConfig           *gDeviceConfig = NULL;


// reset func
typedef void         (*ResetFuncPtr)();
ResetFuncPtr         reset = NULL;



/// ISR for timer overflow (NOTE: we cannot use delay() here since the timers might be disabled)
ISR(WDT_vect)
{
    noInterrupts();
    
    // flag a timer event after TMR_OVERFLOW_COUNT number of ISR calls
    gTimeEventCounter++;
    if (gTimeEventCounter >= TMR_OVERFLOW_COUNT)
    {
        gTimeEvent = true;
        gTimeEventCounter = 0;
    }
    
    gSensorEventsEnabled = true;
    interrupts();
}


/// ISR for pin interrupt 0 (NOTE: we cannot use delay() here since the timers might be disabled)
void interruptEvtD2()
{
    if (gSensorEventsEnabled == true)
    {
        // perform simple debouncing
        gSensorEventD2 = true;
        for (int i = 0; i < 512; i++)
        {
            gSensorEventD2 &= digitalRead(EVTD2_INT_PIN) == HIGH;
        }
        
        if (gSensorEventD2 == true)
        {
            gSensorEventsEnabled = false; // disable sensor events until next timer event to limit the frequency of sensor events
            gTimeEventCounter = 0;    // timer event is used as a keepalive and is not required when there are sensor events  
        }
    }
}


/// ISR for pin interrupt 1 (NOTE: we cannot use delay() here since the timers might be disabled)
void interruptEvtD3()
{
    if (gSensorEventsEnabled == true)
    {
        // perform simple debouncing
        gSensorEventD3 = true;
        for (int i = 0; i < 512; i++)
        {
            gSensorEventD3 &= digitalRead(EVTD3_INT_PIN) == HIGH;
        }
        
        if (gSensorEventD3 == true)
        {
            gSensorEventsEnabled = false; // disable sensor events until next timer event to limit the frequency of sensor events
            gTimeEventCounter = 0;    // timer event is used as a keepalive and is not required when there are sensor events  
        }
    }
}


/// creates radio message
void createDeviceMsg(char *_pszMessage, unsigned char _uMsgSize, bool _bTimeEvent, bool _bD2Event, bool _bD3Event)
{
    float fBatteryVoltage = BTY_RESOLUTION * (float)analogRead(BTY_PIN) / BTY_DEVIDER;
    float fChargeVoltage = CHG_RESOLUTION * (float)analogRead(CHG_PIN) / CHG_DEVIDER;
    float fTemperature = TMP_RESOLUTION * (float)analogRead(TMP_PIN);
    
    sDeviceData &gDevice = gDeviceConfig->config();
    gDevice._uBtyVoltage = (unsigned short)(fBatteryVoltage*100 + 0.5);
    gDevice._uChgVoltage = (unsigned short)(fChargeVoltage*100 + 0.5);
    gDevice._uTemperature = (unsigned char)(fTemperature + 0.5);
    gDevice._bTimeEvent = _bTimeEvent;
    gDevice._bD2Event = _bD2Event;
    gDevice._bD3Event = _bD3Event;
    gDevice._uEventCount++;
    
    encodeDeviceData(_pszMessage, _uMsgSize, gDevice);
}


/// sends radio message (will send the message twice and then sleep the radio)
void sendDeviceMessage(const char *_pszMessage)
{
    // wake up radio
    gRadio->sleep(false);
    
    // send message twice
    gRadio->stream().println(_pszMessage);
    waitAndFlash(50, 10, DEVICE_STATUS_LED_PIN);
    gRadio->stream().println(_pszMessage);

    // sleep radio and wait to make sure device is sleeping
    gRadio->sleep(true);
    waitAndFlash(50, 10, DEVICE_STATUS_LED_PIN);
}


// sleep device (will wake up on timer interrupt, pin interrupt or WDT)
void sleepNow()
{
    // switch functions off before we sleep
    digitalWrite(DEVICE_STATUS_LED_PIN, LOW);
    gRadio->sleep(true);
    
    // shut down ADC
    byte oldADCSRA = ADCSRA;
    ADCSRA = 0;
    
    // turn off brown-out enable in software
    MCUCR = _BV (BODS) | _BV (BODSE);  // turn on brown-out enable select
    MCUCR = _BV (BODS);        // this must be done within 4 clock cycles of above
    
    // go to sleep (will wake on interrupts)
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    sleep_enable();
    sleep_mode();

    // execution starts here when device wakes
    sleep_disable();
    
    // switch on ADC
    ADCSRA = oldADCSRA;
}


/// arduino device setup
void setup()
{
    // allow for a programming delay before pins and interrupts are changed
    waitAndFlash(4000, 500, DEVICE_STATUS_LED_PIN);

    // set Fio pins 4 to 13 as low outputs to save power (leaves RX, TX and sensor input pins alone)
    for (unsigned char i = 4; i <= 13; i++)
    {
        pinMode(i, OUTPUT);
        digitalWrite(i, LOW);
    }
    
    // setup status LED output pin
    pinMode(DEVICE_STATUS_LED_PIN, OUTPUT);
    digitalWrite(DEVICE_STATUS_LED_PIN, LOW);
  
    // setup sensor input pins
    pinMode(EVTD2_INT_PIN, INPUT_PULLUP);
    pinMode(EVTD3_INT_PIN, INPUT_PULLUP);
    
    attachInterrupt(EVTD2_INT_NO, interruptEvtD2, RISING);    
    attachInterrupt(EVTD3_INT_NO, interruptEvtD3, RISING);

    // setup radio module
    Serial.begin(RADIO_BAUD);
    gRadio = new FioXBee(Serial, RADIO_BAUD, RADIO_SLEEP_PIN);
    
    // load device config
    gDeviceConfig = new DeviceConfig(Serial, DEVICE_STATUS_LED_PIN);
    gDeviceConfig->loadFromEeprom();
    
    // wait for, read and process configuration commands
    if (gDeviceConfig->waitForConfig(10000) == true)
    {
        gDeviceConfig->storeToEeprom();
    }
            
    // program radio module with new config
    waitAndFlash(1000, 200, DEVICE_STATUS_LED_PIN);
    digitalWrite(DEVICE_STATUS_LED_PIN, HIGH);
    if (gRadio->program(gDeviceConfig->config()._uAddr, RADIO_PAN_ID) == false)
    {
        waitAndFlash(1000, 200, DEVICE_STATUS_LED_PIN);
        
        // retry with default radio baud rate if programming failed
        Serial.begin(9600);
        gRadio->program(gDeviceConfig->config()._uAddr, RADIO_PAN_ID);
        
        // reset baud rate
        Serial.begin(RADIO_BAUD);
    }
    
    digitalWrite(DEVICE_STATUS_LED_PIN, LOW);
    
    // output first message
    char pszOutput[OUTPUT_BUF_SIZE];
    createDeviceMsg(pszOutput, OUTPUT_BUF_SIZE, false, false, false);
    sendDeviceMessage(pszOutput);

    // wait and init event flags 
    waitAndFlash(1000, 200, DEVICE_STATUS_LED_PIN);
    gTimeEvent = false;
    gSensorEventD2 = false;
    gSensorEventD3 = false;

    // turn on watchdog timer and enable interrupt
    noInterrupts();
    MCUSR = 0;
    WDTCSR |= B00011000;
    WDTCSR = B01100001;                  // 8 Second Timeout
    interrupts();
}


/// arduino loop (look for events and sleep device if nothing is going on)
void loop()
{
    if ( (gTimeEvent == true) || (gSensorEventD2 == true)  || (gSensorEventD3 == true) )
    {
        // create message and send message
        char pszOutput[OUTPUT_BUF_SIZE];
        createDeviceMsg(pszOutput, OUTPUT_BUF_SIZE, gTimeEvent, gSensorEventD2, gSensorEventD3);          
        sendDeviceMessage(pszOutput);
        
        // reset events
        // NOTE: radio can cause false sensor events, so reset events here 
        gSensorEventD2 = false;
        gSensorEventD3 = false;
        gTimeEvent = false;
    }
    else  // go to sleep
    {
        sleepNow();        
    }
}


