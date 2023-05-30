#include "storage.h"
#include "flowmeter.h"
#include "MQTT_task.h"

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
    Serial.print("Setting up Hardware Timer for backup to EEPROM with period: ");
    TIM_TypeDef *EEPROMTimerInstance = TIM2;
    HardwareTimer *EEPROMStore = new HardwareTimer(EEPROMTimerInstance);
    EEPROMStore->pause();
    EEPROMStore->setPrescaleFactor(65536);
    EEPROMStore->setOverflow(4194304);
    Serial.print(EEPROMStore->getOverflow() / (EEPROMStore->getTimerClkFreq() / EEPROMStore->getPrescaleFactor()) / 60);
    Serial.println(" min");
    EEPROMStore->refresh();
    EEPROMStore->resume();
    EEPROMStore->attachInterrupt(BackupEEPROMPut);
  }
  else {
    Serial.println("ERROR: Can't find eeprom\nstopped...");
    while (1);
  }
  
  Serial.println("Initialize RTC backup registry: ");
  enableBackupDomain();
  Serial.print("Setting up Hardware Timer for backup to RTC with period: ");
  TIM_TypeDef *RTCTimerInstance = TIM5;
  HardwareTimer *RTCStore = new HardwareTimer(RTCTimerInstance);
  RTCStore->pause();
  RTCStore->setPrescaleFactor(65536);
  RTCStore->setOverflow(1048576);
  Serial.print(RTCStore->getOverflow() / (RTCStore->getTimerClkFreq() / RTCStore->getPrescaleFactor()) / 60);
  Serial.println(" min");
  RTCStore->refresh();
  RTCStore->resume();
  RTCStore->attachInterrupt(BackupRTCPut); //backup to RTC
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
  Serial.print("Storing current meter data to EEPROM with epoch: ");
  Serial.println(ActualData.Timestamp);
  flash_mem.write(ActualData);
}

void BackupRTCPut()
{
  ActualData.Timestamp = rtc.getEpoch();
  Serial.print("Storing current meter data to RTC with epoch: ");
  Serial.println(ActualData.Timestamp);
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
    Serial.println("Restored data from RTC");
    ActualData.WaterConsumption = RTCData.WaterConsumption;
    ActualData.WaterConsumptionFilter1 = RTCData.WaterConsumptionFilter1;
    ActualData.WaterConsumptionFilter2 = RTCData.WaterConsumptionFilter2;
    ActualData.WaterConsumptionFilter3 = RTCData.WaterConsumptionFilter3;
  }
  else {
    Serial.println("Restored data from EEPROM");
    ActualData.WaterConsumption = EEPROMData.WaterConsumption;
    ActualData.WaterConsumptionFilter1 = EEPROMData.WaterConsumptionFilter1;
    ActualData.WaterConsumptionFilter2 = EEPROMData.WaterConsumptionFilter2;
    ActualData.WaterConsumptionFilter3 = EEPROMData.WaterConsumptionFilter3;
  }
  Serial.print("Consumption: ");
  Serial.print(ActualData.WaterConsumption);
  Serial.print(", Filter 1: ");
  Serial.print(ActualData.WaterConsumptionFilter1);
  Serial.print(", Filter 2: ");
  Serial.print(ActualData.WaterConsumptionFilter2);
  Serial.print(", Filter 3: ");
  Serial.println(ActualData.WaterConsumptionFilter3);
}
