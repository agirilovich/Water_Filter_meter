uint32_t channel;
volatile uint32_t FrequencyMeasured, LastCapture = 0, CurrentCapture;
uint32_t input_freq = 0;
volatile uint32_t rolloverCompareCount = 0;
HardwareTimer *FlowTimer;

void FlowMeterInit(const char *pin)
{
  // Automatically retrieve TIM instance and channel associated to pin
  TIM_TypeDef *Instance = (TIM_TypeDef *)pinmap_peripheral(digitalPinToPinName(pin), PinMap_PWM);
  channel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(pin), PinMap_PWM));

  // Instantiate HardwareTimer object.
  FlowTimer = new HardwareTimer(Instance);

  // Configure rising edge detection to measure frequency
  FlowTimer->setMode(channel, TIMER_INPUT_CAPTURE_RISING, pin);

  uint32_t PrescalerFactor = 1;
  FlowTimer->setPrescaleFactor(PrescalerFactor);
  FlowTimer->setOverflow(0x10000); // Max Period value to have the largest possible time to detect rising edge and avoid timer rollover
  FlowTimer->attachInterrupt(channel, InputCapture_IT_callback);
  FlowTimer->attachInterrupt(Rollover_IT_callback);
  FlowTimer->resume();
}

void InputCapture_IT_callback(void)
{
  CurrentCapture = FlowTimer->getCaptureCompare(channel);
  /* frequency computation */
  if (CurrentCapture > LastCapture) {
    FrequencyMeasured = input_freq / (CurrentCapture - LastCapture);
  }
  else if (CurrentCapture <= LastCapture) {
    /* 0x1000 is max overflow value */
    FrequencyMeasured = input_freq / (0x10000 + CurrentCapture - LastCapture);
  }
  LastCapture = CurrentCapture;
  rolloverCompareCount = 0;
}

void Rollover_IT_callback(void)
{
  rolloverCompareCount++;

  if (rolloverCompareCount > 1)
  {
    FrequencyMeasured = 0;
  }
}