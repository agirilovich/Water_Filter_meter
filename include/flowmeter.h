#include <Arduino.h>
#include <stdint.h>

#define FlowSensorPin PA15

#define NTC_REFERENCE_RESISTANCE   68800
#define NTC_NOMINAL_RESISTANCE     50000
#define NTC_NOMINAL_TEMPERATURE    25
#define NTC_B_VALUE                3950
//#define STM32_ANALOG_RESOLUTION 4095
#define STM32_ANALOG_RESOLUTION 1024


#define NTC_SENSOR_PIN PB0

#define TdsSensorPin PB1

#define TDSArrayLenght 50

void FlowMeterInit(void);
void NTCSensorInit(void);
void TDSSensorInit(void);

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