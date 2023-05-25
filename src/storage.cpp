#include "storage.h"
#include "flowmeter.h"

#include <Wire.h>
#include <I2C_eeprom.h>
#include <I2C_eeprom_cyclic_store.h>

#include <STM32RTC.h>
#include <backup.h>

TwoWire Wire1(PB9, PB8);
I2C_eeprom ee(0x50, I2C_DEVICESIZE_24LC256, &Wire1);
I2C_eeprom_cyclic_store<FlowMeterData> flash_mem;

STM32RTC& rtc = STM32RTC::getInstance();


void BackupInit()
{
  ee.begin();
  if (ee.isConnected())
  {
    flash_mem.begin(ee, PAGE_SIZE, I2C_DEVICESIZE_24LC256/PAGE_SIZE);
    uint16_t slots;
    uint32_t writes;
    flash_mem.getMetrics(slots, writes);
    Serial.println("EEPROM has been initialized");
    Serial.print("Write counter: ");
    Serial.println(writes);
  }
  else {
    Serial.println("ERROR: Can't find eeprom\nstopped...");
    while (1);
  }
  
  enableBackupDomain(); //enable RTC backup registry
}

void RTCInit(unsigned long EpochTime)
{
  rtc.begin(); // initialize RTC 24H format
  Serial.print("Set RTC on:  ");
  Serial.println(EpochTime);
  rtc.setEpoch(EpochTime);
}

void BackupEEPROMPut()
{
  ActualData.Timestamp = rtc.getEpoch();
  flash_mem.write(ActualData);
}

void BackupRTCPut()
{
  ActualData.Timestamp = rtc.getEpoch();
  setBackupRegister(0, ActualData.Timestamp);
  setBackupRegister(1, ActualData.WaterConsumption);
  setBackupRegister(2, ActualData.WaterConsumptionFilter1);
  setBackupRegister(3, ActualData.WaterConsumptionFilter2);
  setBackupRegister(4, ActualData.WaterConsumptionFilter3);
}

void BackupGet()
{
  struct FlowMeterData EEPROMData;
  struct FlowMeterData RTCData;
  flash_mem.read(EEPROMData);
  RTCData.Timestamp = getBackupRegister(0);
  RTCData.WaterConsumption = getBackupRegister(1);
  RTCData.WaterConsumptionFilter1 = getBackupRegister(2);
  RTCData.WaterConsumptionFilter2 = getBackupRegister(3);
  RTCData.WaterConsumptionFilter3 = getBackupRegister(4);
  Serial.print("Timestamp from EEPROM: ");
  Serial.println(EEPROMData.Timestamp);
  Serial.print("Timestamp from RTC: ");
  Serial.println(RTCData.Timestamp);
  if (RTCData.Timestamp >= EEPROMData.Timestamp)
  {
    ActualData.WaterConsumption = RTCData.WaterConsumption;
    ActualData.WaterConsumptionFilter1 = RTCData.WaterConsumptionFilter1;
    ActualData.WaterConsumptionFilter2 = RTCData.WaterConsumptionFilter2;
    ActualData.WaterConsumptionFilter3 = RTCData.WaterConsumptionFilter3;
  }
  else {
    ActualData.WaterConsumption = EEPROMData.WaterConsumption;
    ActualData.WaterConsumptionFilter1 = EEPROMData.WaterConsumptionFilter1;
    ActualData.WaterConsumptionFilter2 = EEPROMData.WaterConsumptionFilter2;
    ActualData.WaterConsumptionFilter3 = EEPROMData.WaterConsumptionFilter3;
  }
}
