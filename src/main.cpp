#include <Arduino.h>

#include <IWatchdog.h>

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "WaterFilter"
#endif

#define DEVICE_HOSTNAME "waterflt"

#define LED_PIN PC13

#define BEEPER_PIN PA7

#define BUTTON_PIN_1 PA4
#define BUTTON_PIN_2 PA5


//objects for button processing
#include <EasyButton.h>
EasyButton button1(BUTTON_PIN_1);
EasyButton button2(BUTTON_PIN_2);
void button1ISR();
void button2ISR();
void Button1Handler();
void Button2Handler();
void Button2HandlerLong();


#include "lcd.h"

#include "storage.h"

#include "controlWiFi.h" 

#include <TimeLib.h>

#include "MQTT_task.h"

//Multitask definitions
#define _TASK_PRIORITY
#define _TASK_TIMECRITICAL
#define _TASK_WDT_IDS

//load all the objects about water meter
#include "flowmeter.h"

void ButtonsUpdateCallback();

/*
Tasker replaced by timers:
TIM1 - LCD loop
TIM2 - EEPROM
TIM3 - Flow
TIM4 - MQTT
TIM5 - RTC
TIM9 - buttons
TIM10 - NTC
TIM11 - TDS
*/

TIM_TypeDef *LCDloopTimerInstance = TIM1;
HardwareTimer *DisplayLoop = new HardwareTimer(LCDloopTimerInstance);

TIM_TypeDef *ButtonsTimerInstance = TIM9;
HardwareTimer *ButtonsThread = new HardwareTimer(ButtonsTimerInstance);

void setup() {
  // Debug console
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  while (!Serial && millis() < 5000);

  if (IWatchdog.isReset()) {
    Serial.printf("Rebooted by Watchdog!\n");
    delay(60 * 1000);
    IWatchdog.clearReset(); 
  }

  //Set witchdog timeout for 32 seconds
  IWatchdog.begin(32000000); // set to maximum value
  IWatchdog.reload();

  while (!IWatchdog.isEnabled()) {
    // LED blinks indefinitely
    digitalWrite(LED_PIN, LOW);
    delay(500);
    digitalWrite(LED_PIN, HIGH);
    delay(500);
  }

  //initialise LCD
  initializeLCD();

  //Set port for beeper
  Serial.print("Test beep signal");
  pinMode(BEEPER_PIN, OUTPUT);
  digitalWrite(BEEPER_PIN, HIGH);
  delay(50);
  digitalWrite(BEEPER_PIN, LOW);

  //Initialise EEPROM flash module and backup registry
  BackupInit();

  Serial.print("Start WiFiMQTT on ");
  Serial.println(DEVICE_BOARD_NAME);
  
  initializeWiFiShield(DEVICE_HOSTNAME);
  Serial.println("WiFi shield init done");

  establishWiFi();

  // you're connected now, so print out the data
  printWifiStatus();

  initMQTT();
  
  //Initialise TDS sensor
  TDSSensorInit();

  //setup NTC sensor
  NTCSensorInit();
  
  //setup flow meter
  FlowMeterInit();

  button1.begin();
  button1.onPressed(Button1Handler);
  button1.enableInterrupt(button1ISR);

  button2.begin();
  button2.onPressed(Button2Handler);
  button2.enableInterrupt(button2ISR);
  button2.onPressedFor(5000, Button2HandlerLong);

  //Set HardwareTimer for buttons update
  Serial.print("Setting up Hardware Timer for buttons update with period: ");
  ButtonsThread->pause();
  ButtonsThread->setPrescaleFactor(2048);
  Serial.print(ButtonsThread->getOverflow() / (ButtonsThread->getTimerClkFreq() / ButtonsThread->getPrescaleFactor()));
  Serial.println(" sec");
  ButtonsThread->attachInterrupt(ButtonsUpdateCallback);
  ButtonsThread->refresh();
  ButtonsThread->resume();
  
  //SYNC RTC with NTP
  Serial.print("Syncronize RTC with NTP server.");
  WiFi.sntp("pl.pool.ntp.org");
  while (WiFi.getTime() < SECS_YR_2000) {
    delay(1000);
    Serial.print('.');
  }
  ActualData.Timestamp = WiFi.getTime();
  Serial.println();
  Serial.print("Received date:  ");
  Serial.println(ActualData.Timestamp);
  setTime(ActualData.Timestamp);
  RTCInit(ActualData.Timestamp);
  
  //Initialise EEPROM flash module, backup registry and restore saved values
  BackupInit();
  BackupGet();

  //Set HardwareTimer for LCD loop
  Serial.print("Setting up Hardware Timer for LCD loop with period: ");
  DisplayLoop->pause();
  DisplayLoop->setPrescaleFactor(32768);
  DisplayLoop->setCount(500);
  Serial.print(DisplayLoop->getOverflow() / (DisplayLoop->getTimerClkFreq() / DisplayLoop->getPrescaleFactor()));
  Serial.println(" sec");
  DisplayLoop->refresh();
  DisplayLoop->attachInterrupt(DisplayLoopCallback);
  DisplayLoop->setCount(0);
  DisplayLoop->resume();

}

void loop()
{
  
}

void ButtonsUpdateCallback()
{
  button2.update();
  //Check MQTT and WiFi status
  if (MQTThealth())
  {
    IWatchdog.reload();
  }
}

void button1ISR()
{
  button1.read();
}

void button2ISR()
{
  button2.read();
}

void Button1Handler()
{
  Serial.println("button 1 was pressed");
  digitalWrite(BEEPER_PIN, HIGH);
  delay(50);
  digitalWrite(BEEPER_PIN, LOW);
  switch(DisplayState)
  {
    case DisplayView::Reset1:
    {
      DisplayState = 7;
      DisplayControlCallback();
      DisplayState = 0;
      DisplayLoop->setCount(0);
      DisplayLoop->resume();
    }
    case DisplayView::Reset2:
    {
      DisplayState = 7;
      DisplayControlCallback();
      DisplayState = 0;
      DisplayLoop->setCount(0);
      DisplayLoop->resume();
    }
    case DisplayView::Reset3:
    {
      DisplayState = 7;
      DisplayControlCallback();
      DisplayState = 0;
      DisplayLoop->setCount(0);
      DisplayLoop->resume();
    }
    case DisplayView::Cancel:
    {
      DisplayLoop->pause();
      DisplayState = 7;
      DisplayControlCallback();
      DisplayState = 0;
      DisplayLoop->setCount(0);
      DisplayLoop->resume();
    }
    case DisplayView::Done:
    {
      DisplayLoop->pause();
      DisplayState = 0;
      DisplayControlCallback();
      DisplayLoop->setCount(0);
      DisplayLoop->resume();
    }
    default:
    {
      DisplayLoopCallback();
    }
  }
}


void Button2HandlerLong()
{
  Serial.println("long button 2 was pressed");
  digitalWrite(BEEPER_PIN, HIGH);
  delay(300);
  digitalWrite(BEEPER_PIN, LOW);
  //pause loop in a display
  DisplayLoop->pause();
  switch(DisplayState)
  {
    case DisplayView::Filter1:
    {
      DisplayState = 8;
      DisplayControlCallback();
      break;
    }
    case DisplayView::Filter2:
    {
      DisplayState = 9;
      DisplayControlCallback();
      break;
    }
    case DisplayView::Filter3:
    {
      DisplayState = 10;
      DisplayControlCallback();
      break;
    }
    default:
    {
      DisplayState = 6;
      DisplayControlCallback();
      DisplayLoop->setCount(0);
      DisplayLoop->resume();
    }
  }
}

void Button2Handler()
{
  Serial.println("button 2 was pressed");
  digitalWrite(BEEPER_PIN, HIGH);
  delay(50);
  digitalWrite(BEEPER_PIN, LOW);
  //pause loop in a display
  DisplayLoop->pause();
  switch(DisplayState)
  {
    case DisplayView::Reset1:
    {
      ActualData.WaterConsumptionFilter1 = 0;
      DisplayState = 7;
      DisplayControlCallback();
      BackupRTCPut();
      break;
    }
    case DisplayView::Reset2:
    {
      ActualData.WaterConsumptionFilter2 = 0;
      DisplayState = 7;
      DisplayControlCallback();
      BackupRTCPut();
      break;
    }
    case DisplayView::Reset3:
    {
      ActualData.WaterConsumptionFilter3 = 0;
      DisplayState = 7;
      DisplayControlCallback();
      BackupRTCPut();
      break;
    }
    default:
    {
      DisplayState = 6;
      DisplayControlCallback();
    }
  }
  DisplayLoop->setCount(0);
  DisplayLoop->resume();
}
