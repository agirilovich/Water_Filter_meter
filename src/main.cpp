#include <Arduino.h>

#include <LiquidCrystal.h>

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

#define MQTT_TOPIC_STATE MQTT_GENERAL_PREFIX "/" DEVICE_BOARD_NAME

#define MQTT_TEMP_TOPIC_STATE MQTT_TOPIC_STATE "/temp/state"
#define MQTT_TDS_TOPIC_STATE MQTT_TOPIC_STATE "/tds/state"
#define MQTT_FLOW_TOPIC_STATE MQTT_TOPIC_STATE "/flow/state"

#define LED_PIN PC13
#define TdsSensorPin PB1
#define FlowSensorPin PB2
#define NTC_SENSOR_PIN PB0

#include "controlWiFi.h" 
WiFiClient client;

#include "MQTT_task.h"
PubSubClient mqtt(client);

//Define MQTT Topic for HomeAssistant Discovery
const char *MQTTTempTopicConfig = MQTT_TOPIC_TEMP_CONFIG;
const char *MQTTTDSTopicConfig = MQTT_TOPIC_TDS_CONFIG;
const char *MQTTFlowTopicConfig = MQTT_TOPIC_FLOW_CONFIG;

//Define MQTT Topic for HomeAssistant Sensor state
const char *MQTTTempTopicState = MQTT_TEMP_TOPIC_STATE;
const char *MQTTTDSTopicState = MQTT_TDS_TOPIC_STATE;
const char *MQTTFlowTopicState = MQTT_FLOW_TOPIC_STATE;

//Define objects for MQTT messages in JSON format
#include <ArduinoJson.h>

StaticJsonDocument<512> JsonSensorConfig;
char Buffer[256];

//Multitask definitions
#include <TaskScheduler.h>
#define _TASK_SLEEP_ON_IDLE_RUN  // Enable 1 ms SLEEP_IDLE powerdowns between runs if no callback methods were invoked during the pass

Scheduler runner;

void TDSSensorCallback();
void FLowMeterCallback();
void TemperatureSensorCallback();
void MQTTMessageCallback();

Task TDSThread(1 * TASK_MINUTE, TASK_FOREVER, &TDSSensorCallback, &runner, true);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task FlowThread(1 * TASK_MINUTE, TASK_FOREVER, &FLowMeterCallback, &runner, true);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task TempThread(10 * TASK_SECOND, TASK_FOREVER, &TDSSensorCallback, &runner, true);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task mqttThread(5 * TASK_MINUTE, TASK_FOREVER, &MQTTMessageCallback, &runner, true);  //Runs every 5 minutes after several measurements of Ultrasonic Sensor


#include "flowmeter.h"
uint64_t WaterConsumption;

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

  //Initialise TDS sensor
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(4096);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization

  //setup flow meter
  FlowMeterInit(FlowSensorPin);

  //setup NTC sensor
  thermistor = new NTC_Thermistor(
    NTC_SENSOR_PIN,
    NTC_REFERENCE_RESISTANCE,
    NTC_NOMINAL_RESISTANCE,
    NTC_NOMINAL_TEMPERATURE,
    NTC_B_VALUE,
    STM32_ANALOG_RESOLUTION // <- for a thermistor calibration
  );

  runner.startNow();  // This creates a new scheduling starting point for all ACTIVE tasks.

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
  if (!publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTTempTopicState, (char*)temperature) || !publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTTDSTopicState, (char*)tdsValue) || !publishMQTTPayload(mqtt, mqtt_user, mqtt_pass, MQTTFlowTopicState, (char*)WaterConsumption) ) {
    runner.disableAll();   //pause runner and wait for watchdog if MQTT is broken
  }
  else {
    Serial.println("Done");
  }
}