#include "storage.h"
#include "flowmeter.h"

#include <Wire.h>
#include <I2C_eeprom.h>
#include <I2C_eeprom_cyclic_store.h>
#include <backup.h>

TwoWire Wire1(PB9, PB8);
I2C_eeprom ee(0x50, I2C_DEVICESIZE_24LC256, &Wire1);
I2C_eeprom_cyclic_store<FlowMeterData> flash_mem;


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
  //enable RTC backup registry
  enableBackupDomain();
}

void BackupEEPROMPut()
{
  flash_mem.write(ActualData);
}


void BackupRTCPut()
{
  setBackupRegister(0, ActualData.TimeStamp);
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
  RTCData.WaterConsumption = getBackupRegister(0);
  RTCData.WaterConsumptionFilter1 = getBackupRegister(1);
  RTCData.WaterConsumptionFilter2 = getBackupRegister(2);
  RTCData.WaterConsumptionFilter3 = getBackupRegister(3);
  ActualData.WaterConsumption = RTCData.WaterConsumption >= EEPROMData.WaterConsumption ? RTCData.WaterConsumption : EEPROMData.WaterConsumption;
  ActualData.WaterConsumptionFilter1 = RTCData.WaterConsumptionFilter1 >= EEPROMData.WaterConsumptionFilter1 ? RTCData.WaterConsumptionFilter1 : EEPROMData.WaterConsumptionFilter1;
  ActualData.WaterConsumptionFilter2 = RTCData.WaterConsumptionFilter2 >= EEPROMData.WaterConsumptionFilter2 ? RTCData.WaterConsumptionFilter2 : EEPROMData.WaterConsumptionFilter2;
  ActualData.WaterConsumptionFilter3 = RTCData.WaterConsumptionFilter3 >= EEPROMData.WaterConsumptionFilter3 ? RTCData.WaterConsumptionFilter3 : EEPROMData.WaterConsumptionFilter3;
  Serial.println("Stored in EEPROM values:");
  Serial.println(EEPROMData.WaterConsumption);
  Serial.println(EEPROMData.WaterConsumptionFilter1);
  Serial.println(EEPROMData.WaterConsumptionFilter2);
  Serial.println(EEPROMData.WaterConsumptionFilter3);
  Serial.println("Stored in RTC values:");
  Serial.println(RTCData.WaterConsumption);
  Serial.println(RTCData.WaterConsumptionFilter1);
  Serial.println(RTCData.WaterConsumptionFilter2);
  Serial.println(RTCData.WaterConsumptionFilter3);
}
