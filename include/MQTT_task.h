#include <PubSubClient.h>

void initializeMQTT(PubSubClient mqtt, const char *mqtt_user, const char *mqtt_pass, const char *Topic, char *SensorConfig);

bool publishMQTTPayload(PubSubClient mqtt, const char *mqtt_user, const char *mqtt_pass, const char *Topic, char *PayloadMessage);


