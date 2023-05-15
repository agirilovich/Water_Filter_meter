#include "MQTT_task.h"

void initializeMQTT(PubSubClient mqtt, const char *mqtt_user, const char *mqtt_pass, const char *Topic, char *SensorConfig)
{

  Serial.print("Testing connection to mqtt broker...");

  while (!mqtt.connect(DEVICE_BOARD_NAME, mqtt_user, mqtt_pass))
  {
    Serial.print(".");
    delay(1000);
  }

  if (mqtt.connected()) {
    Serial.println(" connected!");
  } 

  Serial.println("Initialise MQTT autodiscovery topic and sensor...");

  //Publish message to AutoDiscovery topic
  if (mqtt.publish(Topic, SensorConfig, true)) {
    Serial.println("Done");
  }
  
  //Gracefully close connection to MQTT broker
  mqtt.disconnect();

}


void publishMQTTPayload(PubSubClient mqtt, const char *mqtt_user, const char *mqtt_pass, const char *Topic, char *PayloadMessage)
{
  mqtt.connect(DEVICE_BOARD_NAME, mqtt_user, mqtt_pass);
  if (mqtt.connected()) {
    mqtt.publish(Topic, PayloadMessage, false);
  } 
  else {
    Serial.println("Unable to connect to MQTT broker");
    Serial.println("Cycle is skipped");
  }
  mqtt.disconnect();
}
