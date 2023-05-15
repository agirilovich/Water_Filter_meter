#include <WiFiEspAT.h>

//Definition for Wi-Fi ESP_AT module
//#define EspSerial       Serial1

#define ESP_RESET_PIN PA8

void printWifiStatus();
void initializeWiFiShield(const char *device_name);
void establishWiFi(const char *ssid, const char *password);
