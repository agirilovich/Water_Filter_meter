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

#define MQTT_TOPIC_CONFIG MQTT_CONFIG_PREFIX MQTT_TOPIC_NAME "/config"
#define MQTT_TOPIC_STATE MQTT_GENERAL_PREFIX "/" DEVICE_BOARD_NAME "/state"

#define LED_PIN PC13
#define TdsSensorPin PB1
#define FlowSensorPin PB2

#include "controlWiFi.h" 
WiFiClient client;

#include "MQTT_task.h"
PubSubClient mqtt(client);

//Define MQTT Topic for HomeAssistant Discovery
const char *MQTTTopicConfig = MQTT_TOPIC_CONFIG;

//Define MQTT Topic for HomeAssistant Sensor state
const char *MQTTTopicState = MQTT_TOPIC_STATE;

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
Task TempThread(10 * TASK_SECOND, TASK_FOREVER, &TDSSensorCallback, &runner, true);  //Initially only task is enabled. It runs every 10 seconds indefinitely.
Task mqttThread(1 * TASK_MINUTE, TASK_FOREVER, &MQTTMessageCallback, &runner, true);  //Runs every 5 minutes after several measurements of Ultrasonic Sensor


#include "GravityTDS.h"
GravityTDS gravityTds;
float tdsValue = 0;
float temperature = 25;

#include "flowmeter.h"
const char *FlowMeterTimerPin = FlowSensorPin;

void setup() {
// Debug console
  Serial.begin(115200);

  JsonSensorConfig["name"] = SENSOR_NAME;
  JsonSensorConfig["device_class"] = "distance";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "mm";
  JsonSensorConfig["state_topic"] = MQTTTopicState;

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
  serializeJson(JsonSensorConfig, Buffer);
  
  initializeMQTT(mqtt, mqtt_user, mqtt_pass, MQTTTopicConfig, Buffer);

  //Initialise TDS sensor
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(4096);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization

  //setup flow meter
  FlowMeterInit(FlowMeterTimerPin);

  
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
  Serial.print(tdsValue,0);
  Serial.println("ppm");
    
  IWatchdog.reload();
}

void FLowMeterCallback()
{
  digitalWrite(LED_PIN, HIGH); // activate gate signal
  delayMicroseconds(1000);    //delay(1);
  digitalWrite(LED_PIN, LOW); // deactivate gate signal
  Serial << "Count = " << timer_get_count(TIMER2) << endl; // should be: 1000µs/1/9MHz = ~9000
  timer_set_count(TIMER2, 0); // reset counter
}