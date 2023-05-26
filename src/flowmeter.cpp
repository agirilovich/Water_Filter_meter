#include "flowmeter.h"
#include <Thermistor.h>
#include <NTC_Thermistor.h>
#include "GravityTDS.h"

GravityTDS gravityTds;

float tdsValue = 0;
float temperature = NTC_NOMINAL_TEMPERATURE;

struct FlowMeterData ActualData = {0, 0, 0, 0};

uint32_t channel;
volatile uint32_t LastCapture = 0, CurrentCapture, PulseCounter = 0;
volatile uint32_t rolloverCompareCount = 0;
const float FlowPulseCharacteristics = 1/(60*13);

HardwareTimer *FlowTimer;

Thermistor* thermistor;


//LWMA values filtration
#include <RunningAverage.h>
RunningAverage TDSArray(TDSArrayLenght);


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
  Serial.println("Initialise Flow Meter Counter...");
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

uint32_t GetFlowCounter(void)
{
  uint32_t CurrentFlow;
  FlowTimer->pauseChannel(channel);
  InputCapture_callback();
  CurrentFlow = PulseCounter * FlowPulseCharacteristics;
  PulseCounter = 0;

  FlowTimer->resumeChannel(channel);

  return CurrentFlow;
}

void FLowMeterCallback()
{
  uint32_t CurrentFlow;
  Serial.println("Read data from Input Counter Hardware Timer...");
  CurrentFlow = GetFlowCounter();
  ActualData.WaterConsumption = ActualData.WaterConsumption + CurrentFlow;
  ActualData.WaterConsumptionFilter1 = ActualData.WaterConsumptionFilter1 + CurrentFlow;
  ActualData.WaterConsumptionFilter2 = ActualData.WaterConsumptionFilter2 + CurrentFlow;
  ActualData.WaterConsumptionFilter3 = ActualData.WaterConsumptionFilter3 + CurrentFlow;
  Serial.print("Received value: ");
  Serial.print(ActualData.WaterConsumption);
  Serial.println(" L");
}


void NTCSensorInit(void)
{
  Serial.println("Initialise NTC Sensor");
  thermistor = new NTC_Thermistor(
    NTC_SENSOR_PIN,
    NTC_REFERENCE_RESISTANCE,
    NTC_NOMINAL_RESISTANCE,
    NTC_NOMINAL_TEMPERATURE,
    NTC_B_VALUE,
    STM32_ANALOG_RESOLUTION // <- for a thermistor calibration
  );
}

void TemperatureSensorCallback()
{
  Serial.println("NTC sensor measure...");
  temperature = thermistor->readCelsius();
  Serial.print("Received value: ");
  Serial.print(temperature);
  Serial.println(" C");
}

void TDSSensorInit(void)
{
  Serial.println("Initialise TDS Sensor");
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(4096);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization
  TDSArray.clear();
}

void TDSSensorCallback()
{
  Serial.println("TDS sensor measure...");
  gravityTds.setTemperature(temperature);  // set the temperature and execute temperature compensation
  gravityTds.update();  //sample and calculate
  float curTDS = gravityTds.getTdsValue(); // then get the value
  Serial.print("Received value: ");
  Serial.print(curTDS);
  Serial.println(" ppm");
  TDSArray.addValue(curTDS); 
  tdsValue = TDSArray.getAverage();
}