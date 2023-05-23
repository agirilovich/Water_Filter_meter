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

uint32_t GetFlowCounter(void);

extern float tdsValue;
extern float temperature;

struct FlowMeterData {
  uint32_t Timestamp;
  uint32_t WaterConsumption;
  uint32_t WaterConsumptionFilter1;
  uint32_t WaterConsumptionFilter2;
  uint32_t WaterConsumptionFilter3;
};

extern struct FlowMeterData ActualData;

void FLowMeterCallback();

void TemperatureSensorCallback();

void TDSSensorCallback();