#include <Arduino.h>
#include <stdint.h>

#define FlowSensorPin PB2

#define NTC_REFERENCE_RESISTANCE   10000
#define NTC_NOMINAL_RESISTANCE     50000
#define NTC_NOMINAL_TEMPERATURE    25
#define NTC_B_VALUE                3950
#define STM32_ANALOG_RESOLUTION 4095

#define NTC_SENSOR_PIN PB0

#define TdsSensorPin PB1

void FlowMeterInit(void);
void NTCSensorInit(void);
void TDSSensorInit(void);

uint64_t GetFlowCounter(void);

extern float tdsValue;
extern float temperature;

struct FlowMeterData {
  uint64_t WaterConsumption = 0;
  uint64_t WaterConsumptionFilter1 = 0;
  uint64_t WaterConsumptionFilter2 = 0;
  uint64_t WaterConsumptionFilter3 = 0;
};

extern FlowMeterData EEPROMData;

void FLowMeterCallback();

void TemperatureSensorCallback();

void TDSSensorCallback();