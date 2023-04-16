#include "controlWiFi.h"

int status = WL_IDLE_STATUS;     // the Wifi radio's status

//define UART2 port
HardwareSerial Serial1(PA10, PA9);

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  // you're connected now, so print out the data
  Serial.print(F("You're connected to the network, IP = "));
  Serial.println(WiFi.localIP());

  Serial.print(F("SSID: "));
  Serial.print(WiFi.SSID());

  // print the received signal strength:
  int32_t rssi = WiFi.RSSI();
  Serial.print(F(", Signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
}

void initializeWiFiShield()
{
    EspSerial.begin(115200);
    WiFi.init(&EspSerial);

    if (WiFi.status() == WL_NO_MODULE) {
      Serial.println();
      Serial.println("Communication with WiFi module failed!");
      // don't continue
      while (true);
    }
}

void establishWiFi(const char *ssid, const char *password)
{

  WiFi.disconnect(); // to clear the way. not persistent
  WiFi.setPersistent(); // set the following WiFi connection as persistent
  WiFi.endAP(); // to disable default automatic start of persistent AP at startup

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


