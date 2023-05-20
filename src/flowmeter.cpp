#include "flowmeter.h"

uint32_t channel;
volatile uint32_t LastCapture = 0, CurrentCapture, PulseCounter = 0;
volatile uint32_t rolloverCompareCount = 0;
const float FlowPulseCharacteristics = 1/(60*13);

HardwareTimer *FlowTimer;

void InputCapture_callback(void)
{
  CurrentCapture = FlowTimer->getCaptureCompare(channel);
  
  if (CurrentCapture > LastCapture) {
    PulseCounter = PulseCounter + CurrentCapture - LastCapture;
  }
  else if (CurrentCapture <= LastCapture) {
    /* 0x1000 is max overflow value */
    PulseCounter = PulseCounter + 0x10000 + CurrentCapture - LastCapture;
  }
  LastCapture = CurrentCapture;
  rolloverCompareCount = 0;
}

void Rollover_callback(void)
{
  rolloverCompareCount++;

  if (rolloverCompareCount > 1)
  {
    CurrentCapture = 0;
  }
}

void FlowMeterInit(void)
{
  // Automatically retrieve TIM instance and channel associated to pin
  TIM_TypeDef *Instance = (TIM_TypeDef *)pinmap_peripheral(digitalPinToPinName(FlowSensorPin), PinMap_PWM);
  channel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(FlowSensorPin), PinMap_PWM));

  // Instantiate HardwareTimer object.
  FlowTimer = new HardwareTimer(Instance);

  // Configure rising edge detection
  FlowTimer->setMode(channel, TIMER_INPUT_CAPTURE_RISING, FlowSensorPin);

  uint32_t PrescalerFactor = 1024;
  FlowTimer->setPrescaleFactor(PrescalerFactor);
  FlowTimer->setOverflow(0x10000); // Max Period value to have the largest possible time to detect rising edge and avoid timer rollover
  FlowTimer->attachInterrupt(channel, InputCapture_callback);
  FlowTimer->attachInterrupt(Rollover_callback);

  Serial.print("Run Hardware Timer with frequency: ");
  Serial.println(FlowTimer->getTimerClkFreq() / FlowTimer->getPrescaleFactor());

  FlowTimer->resume();
}

uint64_t GetFlowCounter(void)
{
  uint64_t CurrentFlow;
  FlowTimer->pauseChannel(channel);
  InputCapture_callback();
  CurrentFlow = PulseCounter * FlowPulseCharacteristics;
  PulseCounter = 0;

  FlowTimer->resumeChannel(channel);

  return CurrentFlow;
}
