#include "storage.h"
#include "flowmeter.h"

#include <Wire.h>
#include <I2C_eeprom.h>
#include <I2C_eeprom_cyclic_store.h>
#include <backup.h>

I2C_eeprom ee(0x50, MEMORY_SIZE);
I2C_eeprom_cyclic_store<FlowMeterData> flash_mem;


void BackupInit()
{
  ee.begin();
  if (ee.isConnected())
  {
    flash_mem.begin(ee, PAGE_SIZE, MEMORY_SIZE/PAGE_SIZE);
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

void BackupPut()
{
  flash_mem.write(ActualData);
  setBackupRegister(0, ActualData.WaterConsumption);
  setBackupRegister(1, ActualData.WaterConsumptionFilter1);
  setBackupRegister(2, ActualData.WaterConsumptionFilter2);
  setBackupRegister(3, ActualData.WaterConsumptionFilter3);
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
}
