#include "controlWiFi.h"
//Import credentials from external file out of git repo
#include <Credentials.h>
const char *ssid = ssid_name;
const char *password = ssid_password;

int status = WL_IDLE_STATUS;     // the Wifi radio's status
//define UART2 port
//HardwareSerial EspSerial(USART1);
HardwareSerial EspSerial(PB7, PB6);

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  // you're connected now, so print out the data
  Serial.print(F("You're connected to the network, IP = "));
  Serial.println(WiFi.localIP());

  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print the received signal strength:
  int32_t rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void initializeWiFiShield(const char *device_name)
{
  EspSerial.begin(115200);
  WiFi.init(EspSerial, ESP_RESET_PIN);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println();
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true) {
      EspSerial.println(WiFi.status());
    }
  }

  char fqdn[13];
  strcpy(fqdn, device_name);
  WiFi.setHostname(fqdn);
}

void establishWiFi()
{
  
  WiFi.disconnect(); // to clear the way. not persistent
  WiFi.setPersistent(); // set the following WiFi connection as persistent
  WiFi.endAP(); // to disable default automatic start of persistent AP at startup
  WiFi.setAutoConnect(true);

  Serial.print(F("Connecting to SSID: "));
  Serial.println(ssid);

  status = WiFi.begin(ssid, password);

  delay(1000);

  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED)
  {
    delay(500);

    // Connect to WPA/WPA2 network
    status = WiFi.status();
  }
}


