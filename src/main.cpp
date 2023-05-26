#include <Arduino.h>

#include <IWatchdog.h>

//Import credentials from external file out of git repo
#include <Credentials.h>
const char *ssid = ssid_name;
const char *password = ssid_password;

const char *mqtt_host = mqtt_server;
const int mqtt_port = 1883;
const char *mqtt_user = mqtt_username;
const char *mqtt_pass = mqtt_password;

#ifndef DEVICE_BOARD_NAME
#  define DEVICE_BOARD_NAME "WaterFilter"
#endif

#ifndef MQTT_GENERAL_PREFIX
#  define MQTT_GENERAL_PREFIX "home"
#endif

#define MQTT_TOPIC_NAME "/sensor/waterfilter"
#define MQTT_CONFIG_PREFIX "homeassistant"
#define SENSOR_NAME "Drink Water Filter Condition"

#define MQTT_TOPIC_TEMP_CONFIG MQTT_CONFIG_PREFIX MQTT_TOPIC_NAME "/temp/config"
#define MQTT_TOPIC_TDS_CONFIG MQTT_CONFIG_PREFIX MQTT_TOPIC_NAME "/tds/config"
#define MQTT_TOPIC_FLOW_CONFIG MQTT_CONFIG_PREFIX MQTT_TOPIC_NAME "/flow/config"
#define MQTT_TOPIC_FILER1_CONFIG MQTT_CONFIG_PREFIX MQTT_TOPIC_NAME "/filter1/config"
#define MQTT_TOPIC_FILER2_CONFIG MQTT_CONFIG_PREFIX MQTT_TOPIC_NAME "/filter2/config"
#define MQTT_TOPIC_FILER3_CONFIG MQTT_CONFIG_PREFIX MQTT_TOPIC_NAME "/filter3/config"

#define MQTT_TOPIC_STATE MQTT_GENERAL_PREFIX "/" DEVICE_BOARD_NAME

#define MQTT_TEMP_TOPIC_STATE MQTT_TOPIC_STATE "/temp/state"
#define MQTT_TDS_TOPIC_STATE MQTT_TOPIC_STATE "/tds/state"
#define MQTT_FLOW_TOPIC_STATE MQTT_TOPIC_STATE "/flow/state"
#define MQTT_FILTER1_TOPIC_STATE MQTT_TOPIC_STATE "/filter1/state"
#define MQTT_FILTER2_TOPIC_STATE MQTT_TOPIC_STATE "/filter2/state"
#define MQTT_FILTER3_TOPIC_STATE MQTT_TOPIC_STATE "/filter3/state"

#define LED_PIN PC13

#define BEEPER_PIN PA7

#define BUTTON_PIN_1 PA4
#define BUTTON_PIN_2 PA5


//objects for button processing
#include <OneButton.h>
OneButton button1(BUTTON_PIN_1, true, false);
OneButton button2(BUTTON_PIN_2, true, false);
void button1ISR();
void button2ISR();
void Button1Handler();
void Button2Handler();
void Button2HandlerLong();

#include "lcd.h"

#include "storage.h"

#include "controlWiFi.h" 
WiFiClient client;

#include <NTPClient.h>
WiFiEspAtUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pl.pool.ntp.org");


#include "MQTT_task.h"
PubSubClient mqtt(client);

//Define MQTT Topic for HomeAssistant Discovery
const char *MQTTTempTopicConfig = MQTT_TOPIC_TEMP_CONFIG;
const char *MQTTTDSTopicConfig = MQTT_TOPIC_TDS_CONFIG;
const char *MQTTFlowTopicConfig = MQTT_TOPIC_FLOW_CONFIG;
const char *MQTTFilter1TopicConfig = MQTT_TOPIC_FILER1_CONFIG;
const char *MQTTFilter2TopicConfig = MQTT_TOPIC_FILER2_CONFIG;
const char *MQTTFilter3TopicConfig = MQTT_TOPIC_FILER3_CONFIG;


//Define MQTT Topic for HomeAssistant Sensor state
const char *MQTTTempTopicState = MQTT_TEMP_TOPIC_STATE;
const char *MQTTTDSTopicState = MQTT_TDS_TOPIC_STATE;
const char *MQTTFlowTopicState = MQTT_FLOW_TOPIC_STATE;
const char *MQTTFlowTopicStateFilter1 = MQTT_FILTER1_TOPIC_STATE;
const char *MQTTFlowTopicStateFilter2 = MQTT_FILTER1_TOPIC_STATE;
const char *MQTTFlowTopicStateFilter3 = MQTT_FILTER1_TOPIC_STATE;

//Define objects for MQTT messages in JSON format
#include <ArduinoJson.h>

StaticJsonDocument<512> JsonSensorConfig;
char Buffer[1024];

//Multitask definitions
#define _TASK_PRIORITY
#define _TASK_TIMECRITICAL
#define _TASK_WDT_IDS
#include <TaskScheduler.h>

//load all the objects about water meter
#include "flowmeter.h"

Scheduler runner; //normal priority scheduler

void MQTTMessageCallback();
void ButtonsUpdateCallback();
void Delayer();

Task TDSThread(1 * TASK_MINUTE, TASK_FOREVER, &TDSSensorCallback, &runner, true);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task FlowThread(1 * TASK_MINUTE, TASK_FOREVER, &FLowMeterCallback, &runner, true);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task TempThread(10 * TASK_SECOND, TASK_FOREVER, &TemperatureSensorCallback, &runner, true);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task DisplayControl(10 * TASK_SECOND, TASK_FOREVER, &DisplayControlCallback, &runner, true);
Task ButtonsUpdate(1 * TASK_SECOND, TASK_FOREVER, &ButtonsUpdateCallback, &runner, true);
Task ThreadDelay(0, TASK_ONCE, &Delayer, &runner, true);  //Delay for first run of MQTT publisher and store.
Task mqttThread(5 * TASK_MINUTE, TASK_FOREVER, &MQTTMessageCallback);
Task RTCStore(10 * TASK_MINUTE, TASK_FOREVER, &BackupRTCPut);
Task EEPROMStore(1 * TASK_HOUR, TASK_FOREVER, &BackupEEPROMPut);


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

  //setup NTC sensor
  NTCSensorInit();

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
  
  initializeWiFiShield(DEVICE_BOARD_NAME);
  Serial.println("WiFi shield init done");

  establishWiFi(ssid, password);

  // you're connected now, so print out the data
  printWifiStatus();
  
  Serial.print("Connecting to MQTT broker host: ");
  Serial.println(mqtt_host);
  
  while (!client.connect(mqtt_host, mqtt_port))
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nConnected!");
  
  
  //Initialise MQTT autodiscovery topic and sensor
  mqtt.setServer(mqtt_host, mqtt_port);

  //NTC sensor
  JsonSensorConfig["name"] = "Water temperature";
  JsonSensorConfig["device_class"] = "temperature";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "C";
  JsonSensorConfig["state_topic"] = MQTTTempTopicState;

  JsonObject device  = JsonSensorConfig.createNestedObject("device");
  device["identifiers"][0] = SENSOR_NAME;
  device["model"] = "DWS-MH-01";
  device["name"] = SENSOR_NAME;
  device["manufacturer"] = "Aliexpress"; 
  device["sw_version"] = "1.0";  
  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTT(mqtt, mqtt_user, mqtt_pass, MQTTTempTopicConfig, Buffer);

  //TDS sensor
  JsonSensorConfig["name"] = "Water TDS";
  JsonSensorConfig["device_class"] = "None";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "ppm";
  JsonSensorConfig["state_topic"] = MQTTTDSTopicState;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTT(mqtt, mqtt_user, mqtt_pass, MQTTTDSTopicConfig, Buffer);

  //Flow sensor
  JsonSensorConfig["name"] = "Water Flow";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "L";
  JsonSensorConfig["state_topic"] = MQTTFlowTopicState;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTT(mqtt, mqtt_user, mqtt_pass, MQTTFlowTopicConfig, Buffer);

  //Filter 1
  JsonSensorConfig["name"] = "Consumption Filter 1";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "L";
  JsonSensorConfig["state_topic"] = MQTTFlowTopicStateFilter1;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTT(mqtt, mqtt_user, mqtt_pass, MQTTFilter1TopicConfig, Buffer);

    //Filter 2
  JsonSensorConfig["name"] = "Consumption Filter 2";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "L";
  JsonSensorConfig["state_topic"] = MQTTFlowTopicStateFilter2;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTT(mqtt, mqtt_user, mqtt_pass, MQTTFilter2TopicConfig, Buffer);

    //Filter 3
  JsonSensorConfig["name"] = "Consumption Filter 3";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "L";
  JsonSensorConfig["state_topic"] = MQTTFlowTopicStateFilter3;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTT(mqtt, mqtt_user, mqtt_pass, MQTTFilter3TopicConfig, Buffer);

  //Initialise TDS sensor
  TDSSensorInit();
  
  //setup flow meter
  FlowMeterInit();

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_1), ButtonsUpdateCallback, CHANGE);
  button1.attachClick(Button1Handler);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_2), ButtonsUpdateCallback, CHANGE);
  button2.attachClick(Button2Handler);
  button2.setPressTicks(5000);
  button2.attachLongPressStart(Button2HandlerLong);

   

  //SYNC RTC with NTP
  timeClient.begin();
  timeClient.forceUpdate();
  Serial.print("Syncronize RTC with NTP server. Received date:  ");
  Serial.println(timeClient.getEpochTime());
  RTCInit(timeClient.getEpochTime());
  
  //Initialise EEPROM flash module, backup registry and restore saved values
  BackupInit();
  BackupGet();

  //runner.enableAll(true);

  runner.startNow();  // This creates a new scheduling starting point for all ACTIVE tasks.
    
}

void loop()
{
  runner.execute();
}

void Delayer()
{
  runner.addTask(mqttThread);
  mqttThread.enableDelayed();

  runner.addTask(RTCStore);
  RTCStore.enableDelayed();

  runner.addTask(EEPROMStore);
  EEPROMStore.enableDelayed();
}


void MQTTMessageCallback()
{
  char MessageBuf[16];
  //Publish MQTT messages
  Serial.println("Publishing MQTT messages...");
  //try to publiosh first message
  sprintf(MessageBuf, "%2.3f", temperature);
  if (!publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTTempTopicState, MessageBuf))
  {
    runner.disableAll();   //pause runner and wait for watchdog if MQTT is broken
  }
  else {
    //keep publishing rest of messages
    sprintf(MessageBuf, "%2.3f", tdsValue);
    publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTTDSTopicState, MessageBuf);
    sprintf(MessageBuf, "%" PRIu64, ActualData.WaterConsumption / 1000);
    publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTFlowTopicState, MessageBuf);
    sprintf(MessageBuf, "%" PRIu64, ActualData.WaterConsumptionFilter1 / 1000);
    publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTFilter1TopicConfig, MessageBuf);
    sprintf(MessageBuf, "%" PRIu64, ActualData.WaterConsumptionFilter2 / 1000);
    publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTFilter2TopicConfig, MessageBuf);
    sprintf(MessageBuf, "%" PRIu64, ActualData.WaterConsumptionFilter3 / 1000);
    Serial.println("Done");
  }
}

void ButtonsUpdateCallback()
{
  button1.tick();
  button2.tick();
  IWatchdog.reload();
  if (timeClient.update()) {
    Serial.println(timeClient.getEpochTime());
  }
}

void button1ISR()
{
  Serial.println("button 1 was pressed");
  if (1)
  {
    //beeper
    digitalWrite(BEEPER_PIN, HIGH);
  }
  else {
    digitalWrite(BEEPER_PIN, LOW);
  }
}

void Button1Handler()
{
  Serial.println("button 1 was pressed");
  switch(DisplayState)
  {
    case DisplayView::Reset1:
    {
      DisplayControl.disable();
      DisplayState = 7;
      DisplayControlCallback();
      DisplayState = 0;
      DisplayControl.enableDelayed(90);
    }
    case DisplayView::Reset2:
    {
      DisplayControl.disable();
      DisplayState = 7;
      DisplayControlCallback();
      DisplayState = 0;
      DisplayControl.enableDelayed(90);
    }
    case DisplayView::Reset3:
    {
      DisplayControl.disable();
      DisplayState = 7;
      DisplayControlCallback();
      DisplayState = 0;
      DisplayControl.enableDelayed(90);
    }
    case DisplayView::Cancel:
    {
      DisplayControl.disable();
      DisplayState = 7;
      DisplayControlCallback();
      DisplayState = 0;
      DisplayControl.enableDelayed(90);
    }
    case DisplayView::Done:
    {
      DisplayControl.disable();
      DisplayState = 0;
      DisplayControlCallback();
      DisplayControl.enableDelayed(90);
    }
    default:
    {
      DisplayControlCallback();
    }
  }
}


void Button2HandlerLong()
{
  //pause loop in a display
  DisplayControl.disable();
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
    }
  }
}

void Button2Handler()
{
  //pause loop in a display
  DisplayControl.disable();
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
  DisplayControl.enableDelayed(30);
}