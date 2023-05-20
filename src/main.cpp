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
#define TdsSensorPin PB1
#define NTC_SENSOR_PIN PB0

#define BUTTON_1 PA4
#define BUTTON_2 PA5


#include "lcd.h"
char DisplayBuf[16];

#include "controlWiFi.h" 
WiFiClient client;

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
char Buffer[256];

//Multitask definitions
#define _TASK_PRIORITY
#define _TASK_TIMECRITICAL
#define _TASK_WDT_IDS
#include <TaskScheduler.h>

Scheduler HPRrunner; //high priority scheduler
Scheduler runner; //normal priority scheduler
Scheduler lowrunner; //low priority scheduler

void TDSSensorCallback();
void FLowMeterCallback();
void TemperatureSensorCallback();
void MQTTMessageCallback();
void DisplayControlCallback();
void DisplayTemperatureCallback();
void DisplayTDSCallback();
void DisplayConsumptionCallback();
void DisplayFilter1Callback();
void DisplayFilter2Callback();
void DisplayFilter3Callback();

Task TDSThread(1 * TASK_MINUTE, TASK_FOREVER, &TDSSensorCallback, &runner, false);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task FlowThread(1 * TASK_MINUTE, TASK_FOREVER, &FLowMeterCallback, &HPRrunner, false);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task TempThread(10 * TASK_SECOND, TASK_FOREVER, &TDSSensorCallback, &runner, false);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task mqttThread(5 * TASK_MINUTE, TASK_FOREVER, &MQTTMessageCallback, &runner, false);  //Runs every 5 minutes after several measurements of Ultrasonic Sensor
Task DisplayControl(10 * TASK_SECOND, TASK_FOREVER, &DisplayControlCallback, &runner, false);
//set of low-priority tasks for display output
Task DisplayTemperature(0, TASK_ONCE, &DisplayTemperatureCallback, &lowrunner, false);
Task DisplayTDS(0, TASK_ONCE, &DisplayTemperatureCallback, &lowrunner, false);
Task DisplayConsumption(0, TASK_ONCE, &DisplayConsumptionCallback, &lowrunner, false);
Task DisplayFilter1(0, TASK_ONCE, &DisplayFilter1Callback, &lowrunner, false);
Task DisplayFilter2(0, TASK_ONCE, &DisplayFilter2Callback, &lowrunner, false);
Task DisplayFilter3(0, TASK_ONCE, &DisplayFilter3Callback, &lowrunner, false);


#include "flowmeter.h"
uint64_t WaterConsumption = 0;
uint64_t WaterConsumptionFilter1 = 0;
uint64_t WaterConsumptionFilter2 = 0;
uint64_t WaterConsumptionFilter3 = 0;

#include <Thermistor.h>
#include <NTC_Thermistor.h>
#define NTC_REFERENCE_RESISTANCE   10000
#define NTC_NOMINAL_RESISTANCE     50000
#define NTC_NOMINAL_TEMPERATURE    25
#define NTC_B_VALUE                3950
#define STM32_ANALOG_RESOLUTION 4095

Thermistor* thermistor;

#include "GravityTDS.h"
GravityTDS gravityTds;
double tdsValue = 0;
double temperature = NTC_NOMINAL_TEMPERATURE;


void setup() {
  // Debug console
  Serial.begin(115200);

  while (!Serial && millis() < 5000);

  IWatchdog.begin(60*1000000);

  if (IWatchdog.isReset()) {
    Serial.printf("Rebooted by Watchdog!\n");
    IWatchdog.clearReset();
  }

  //initialise LCD
  initializeLCD();

  Serial.print(F("\nStart WiFiMQTT on "));
  Serial.print(DEVICE_BOARD_NAME);
  
  initializeWiFiShield(DEVICE_BOARD_NAME);
  Serial.println("WiFi shield init done");

  Serial.print(F("Connecting to WiFi network"));
  Serial.print(ssid);
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
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(4096);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization

  //setup flow meter
  FlowMeterInit();

  //setup NTC sensor
  thermistor = new NTC_Thermistor(
    NTC_SENSOR_PIN,
    NTC_REFERENCE_RESISTANCE,
    NTC_NOMINAL_RESISTANCE,
    NTC_NOMINAL_TEMPERATURE,
    NTC_B_VALUE,
    STM32_ANALOG_RESOLUTION // <- for a thermistor calibration
  );

  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_1), Button1Handler, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_2), Button2Handler, FALLING);

  lowrunner.setHighPriorityScheduler(&runner);
  runner.setHighPriorityScheduler(&HPRrunner);
  HPRrunner.enableAll(true);

  runner.enableAll(true);

  //runner.startNow();  // This creates a new scheduling starting point for all ACTIVE tasks.
  
}

void loop() {
  runner.execute();
}



void TDSSensorCallback()
{
  Serial.println("TDS sensor measure...");
  gravityTds.setTemperature(temperature);  // set the temperature and execute temperature compensation
  gravityTds.update();  //sample and calculate
  tdsValue = gravityTds.getTdsValue();  // then get the value
  Serial.print("Received value: ");
  Serial.print(tdsValue);
  Serial.println(" ppm");
    
  IWatchdog.reload();
}

void FLowMeterCallback()
{
  Serial.println("Read data from Input Counter Hardware Timer...");
  WaterConsumption = WaterConsumption + GetFlowCounter();
  WaterConsumptionFilter1 = WaterConsumptionFilter1 + GetFlowCounter();
  WaterConsumptionFilter2 = WaterConsumptionFilter2 + GetFlowCounter();
  WaterConsumptionFilter3 = WaterConsumptionFilter3 + GetFlowCounter();
  Serial.print("Received value: ");
  Serial.print(WaterConsumption);
  Serial.println(" L");

  IWatchdog.reload();
}

void TemperatureSensorCallback()
{
  Serial.println("NTC sensor measure...");
  temperature = thermistor->readCelsius();
  Serial.print("Received value: ");
  Serial.print(temperature);
  Serial.println(" C");

  IWatchdog.reload();
}

void MQTTMessageCallback()
{
  //Publish MQTT messages
  Serial.println("Publishing MQTT messages...");
  //try to publiosh first message
  sprintf(DisplayBuf, "%2.3f", temperature);
  if (!publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTTempTopicState, DisplayBuf))
  {
    runner.disableAll();   //pause runner and wait for watchdog if MQTT is broken
  }
  else {
    //keep publishing rest of messages
    sprintf(DisplayBuf, "%2.3f", tdsValue);
    publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTTDSTopicState, DisplayBuf);
    sprintf(DisplayBuf, "%" PRIu64, WaterConsumption / 1000);
    publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTFlowTopicState, DisplayBuf);
    sprintf(DisplayBuf, "%" PRIu64, WaterConsumptionFilter1 / 1000);
    publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTFilter1TopicConfig, DisplayBuf);
    sprintf(DisplayBuf, "%" PRIu64, WaterConsumptionFilter2 / 1000);
    publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTFilter2TopicConfig, DisplayBuf);
    sprintf(DisplayBuf, "%" PRIu64, WaterConsumptionFilter3 / 1000);
    Serial.println("Done");
  }
}

void DisplayControlCallback(void)
{
  int Interval = 30;
  if (!DisplayTemperature.isEnabled() && !DisplayTDS.isEnabled() && !DisplayConsumption.isEnabled() && !DisplayFilter1.isEnabled() && !DisplayFilter2.isEnabled() && !DisplayFilter3.isEnabled())
  {
    DisplayTemperature.enableDelayed(1 * Interval);
    DisplayTDS.enableDelayed(2 * Interval);
    DisplayConsumption.enableDelayed(3 * Interval);
    DisplayFilter1.enableDelayed(4 * Interval);
    DisplayFilter2.enableDelayed(5 * Interval);
    DisplayFilter3.enableDelayed(6 * Interval);
  }
}

void DisplayTemperatureCallback()
{
  char Title[16] = "Temperature, C:";
  sprintf(DisplayBuf, "%2.3f", temperature);
  clearLCD();
  printLCD(1, Title);
  printLCD(2, DisplayBuf);
}

void DisplayTDSCallback()
{
  char Title[16] = "Water TDS, ppm:";
  sprintf(DisplayBuf, "%2.3f", tdsValue);
  clearLCD();
  printLCD(1, Title);
  printLCD(2, DisplayBuf);
}

void DisplayConsumptionCallback()
{
  char Title[16] = "Consumption, L:";
  sprintf(DisplayBuf, "%" PRIu64, WaterConsumption / 1000);
  clearLCD();
  printLCD(1, Title);
  printLCD(2, DisplayBuf);
}

void DisplayFilter1Callback()
{
  char Title[16] = "Filter 1, L:";
  sprintf(DisplayBuf, "%" PRIu64, WaterConsumptionFilter1 / 1000);
  clearLCD();
  printLCD(1, Title);
  printLCD(2, DisplayBuf);
}

void DisplayFilter2Callback()
{
  char Title[16] = "Filter 2, L:";
  sprintf(DisplayBuf, "%" PRIu64, WaterConsumptionFilter2 / 1000);
  clearLCD();
  printLCD(1, Title);
  printLCD(2, DisplayBuf);
}

void DisplayFilter3Callback()
{
  char Title[16] = "Filter 3, L:";
  sprintf(DisplayBuf, "%" PRIu64, WaterConsumptionFilter3 / 1000);
  clearLCD();
  printLCD(1, Title);
  printLCD(2, DisplayBuf);
}
