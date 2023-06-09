#include "MQTT_task.h"
#include "flowmeter.h"
#include "controlWiFi.h"
#include "Credentials.h"

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
const char *MQTTFlowTopicStateFilter2 = MQTT_FILTER2_TOPIC_STATE;
const char *MQTTFlowTopicStateFilter3 = MQTT_FILTER3_TOPIC_STATE;

const char *mqtt_host = mqtt_server;
const int mqtt_port = 1883;
const char *mqtt_user = mqtt_username;
const char *mqtt_pass = mqtt_password;

int failedMQTTpublish = 0;

//Define objects for MQTT messages in JSON format
#include <ArduinoJson.h>
StaticJsonDocument<512> JsonSensorConfig;
char Buffer[1024];

WiFiClient client;

#include <PubSubClient.h>
PubSubClient mqtt(client);


void initMQTT()
{
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

  Serial.print("Testing connection to mqtt broker...");
  while (!mqtt.connect(DEVICE_BOARD_NAME, mqtt_user, mqtt_pass))
  {
    Serial.print(".");
    delay(1000);
  }

  if (mqtt.connected()) {
    Serial.println(" connected!");
  } 

  //NTC sensor
  JsonSensorConfig["name"] = "Water temperature";
  JsonSensorConfig["device_class"] = "temperature";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "C";
  JsonSensorConfig["uniq_id"] = "waterfiltertempC";
  JsonSensorConfig["state_topic"] = MQTTTempTopicState;

  JsonObject device  = JsonSensorConfig.createNestedObject("device");
  device["identifiers"][0] = "waterfilter000";
  device["connections"][0][0] = "mac";
  device["connections"][0][1] = "BC:FF:4D:F9:D5:13";
  device["model"] = "DWS-MH-01";
  device["name"] = SENSOR_NAME;
  device["manufacturer"] = "Aliexpress"; 
  device["sw_version"] = "1.0";  
  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTTTopic(MQTTTempTopicConfig, Buffer);

  //TDS sensor
  JsonSensorConfig["name"] = "Water TDS";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "ppm";
  JsonSensorConfig["uniq_id"] = "waterfiltertdsppm";
  JsonSensorConfig["state_topic"] = MQTTTDSTopicState;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTTTopic(MQTTTDSTopicConfig, Buffer);

  //Flow sensor
  JsonSensorConfig["name"] = "Water Flow";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "mL";
  JsonSensorConfig["uniq_id"] = "waterfilterflowml";
  JsonSensorConfig["state_topic"] = MQTTFlowTopicState;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTTTopic(MQTTFlowTopicConfig, Buffer);

  //Filter 1
  JsonSensorConfig["name"] = "Consumption Filter 1";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "L";
  JsonSensorConfig["uniq_id"] = "waterfilterfilter1L";
  JsonSensorConfig["state_topic"] = MQTTFlowTopicStateFilter1;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTTTopic(MQTTFilter1TopicConfig, Buffer);

    //Filter 2
  JsonSensorConfig["name"] = "Consumption Filter 2";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "L";
  JsonSensorConfig["uniq_id"] = "waterfilterfilter2L";
  JsonSensorConfig["state_topic"] = MQTTFlowTopicStateFilter2;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTTTopic(MQTTFilter2TopicConfig, Buffer);

    //Filter 3
  JsonSensorConfig["name"] = "Consumption Filter 3";
  JsonSensorConfig["device_class"] = "water";
  JsonSensorConfig["state_class"] = "measurement";
  JsonSensorConfig["unit_of_measurement"] = "L";
  JsonSensorConfig["uniq_id"] = "waterfilterfilter3L";
  JsonSensorConfig["state_topic"] = MQTTFlowTopicStateFilter3;

  serializeJson(JsonSensorConfig, Buffer);
  initializeMQTTTopic(MQTTFilter3TopicConfig, Buffer);

  Serial.print("Setting up Hardware Timer for backup to publish to MQTT with period: ");
  TIM_TypeDef *MQTTimerInstance = TIM4;
  HardwareTimer *mqttThread = new HardwareTimer(MQTTimerInstance);
  mqttThread->pause();
  mqttThread->setPrescaleFactor(65536);
  Serial.print(mqttThread->getOverflow() / (mqttThread->getTimerClkFreq() / mqttThread->getPrescaleFactor()));
  Serial.println(" sec");
  mqttThread->refresh();
  mqttThread->resume();
  mqttThread->attachInterrupt(MQTTMessageCallback);
}

void initializeMQTTTopic(const char *Topic, char *SensorConfig)
{

  Serial.println("Initialise MQTT autodiscovery topics and sensors...");
  Serial.println(Topic);

  //Publish message to AutoDiscovery topic
  if (mqtt.publish(Topic, SensorConfig, true)) {
    Serial.println("Done");
  }
  
  //Gracefully close connection to MQTT broker
}

void MQTTMessageCallback()
{
  char MessageBuf[16];
  //Publish MQTT messages
  Serial.println("Publishing MQTT messages...");
  mqtt.connect(DEVICE_BOARD_NAME, mqtt_user, mqtt_pass);
  if (mqtt.connected()) {
    failedMQTTpublish = 0;

    sprintf(MessageBuf, "%d", int(temperature));
    mqtt.publish(MQTTTempTopicState, MessageBuf, false);
    sprintf(MessageBuf, "%d", int(tdsValue));
    mqtt.publish(MQTTTDSTopicState, MessageBuf, false);
    sprintf(MessageBuf, "%d", int(ActualData.WaterConsumption));
    mqtt.publish(MQTTFlowTopicState, MessageBuf, false);
    sprintf(MessageBuf, "%d", int(ActualData.WaterConsumptionFilter1 / 1000));
    mqtt.publish(MQTTFlowTopicStateFilter1, MessageBuf, false);
    sprintf(MessageBuf, "%d", int(ActualData.WaterConsumptionFilter2 / 1000));
    mqtt.publish(MQTTFlowTopicStateFilter2, MessageBuf, false);
    sprintf(MessageBuf, "%d", int(ActualData.WaterConsumptionFilter3 / 1000));
    mqtt.publish(MQTTFlowTopicStateFilter3, MessageBuf, false);
  }
  else {
    failedMQTTpublish++;
    Serial.println("Unable to connect to MQTT broker");
    Serial.println("Cycle is skipped");
    Serial.print("Number of failed attempts in a chain: ");
    Serial.println(failedMQTTpublish);
  }
  mqtt.disconnect();
  Serial.println("Done");
}

bool MQTThealth()
{
  if (failedMQTTpublish < LIMIT_MQTT_FAILURE)
  {
    return true;
  } else {
    return false;
  }
}