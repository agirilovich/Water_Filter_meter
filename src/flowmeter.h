#include <Arduino.h>
#include <stdint.h>

#define FlowSensorPin PB2

void FlowMeterInit(void);
uint64_t GetFlowCounter(void);
