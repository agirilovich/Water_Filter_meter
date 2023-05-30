#include "flowmeter.h"
#include "GravityTDS.h"

GravityTDS gravityTds;

float tdsValue = 0;
float temperature = NTC_NOMINAL_TEMPERATURE;

uint32_t PulseCounter = 0;

struct FlowMeterData ActualData = {0, 0, 0, 0};

const long double FlowPulseCharacteristics = 1.2820513;

//LWMA values filtration
#include <RunningAverage.h>
RunningAverage TDSArray(TDSArrayLenght);


void pulse_ISR()
{
  PulseCounter++;
}

void FlowMeterInit(void)
{
  Serial.print("Setting up Hardware Timer for Flow Sensor with period: ");
  TIM_TypeDef *FlowTimerInstance = TIM3;
  HardwareTimer *FlowThread = new HardwareTimer(FlowTimerInstance);
  FlowThread->pause();
  FlowThread->setPrescaleFactor(65536);
  Serial.print(FlowThread->getOverflow() / (FlowThread->getTimerClkFreq() / FlowThread->getPrescaleFactor()));
  Serial.println(" sec");
  FlowThread->attachInterrupt(FLowMeterCallback);
  FlowThread->refresh();
  
  pinMode(FlowSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FlowSensorPin), pulse_ISR, FALLING);

  Serial.println("Initialise Flow Meter Counter...");
  
  //FlowCounter->resume();
  FlowThread->resume();
}

void FLowMeterCallback()
{
  Serial.println("Read data from Input Counter Hardware Timer...");
  long double CurrentFlow = PulseCounter * FlowPulseCharacteristics;
  Serial.print("Counted pulses: ");
  Serial.println(PulseCounter);
  PulseCounter = 0;
  ActualData.WaterConsumption = ActualData.WaterConsumption + CurrentFlow;
  ActualData.WaterConsumptionFilter1 = ActualData.WaterConsumptionFilter1 + CurrentFlow;
  ActualData.WaterConsumptionFilter2 = ActualData.WaterConsumptionFilter2 + CurrentFlow;
  ActualData.WaterConsumptionFilter3 = ActualData.WaterConsumptionFilter3 + CurrentFlow;
  Serial.print("Received value: ");
  Serial.print(int(CurrentFlow));
  Serial.print(".");
  Serial.print(int((CurrentFlow - int(CurrentFlow)) * 10000));
  Serial.println(" mL");
}


void NTCSensorInit(void)
{
  Serial.println("Initialise NTC Sensor");
  analogReadResolution(12);
  pinMode(NTC_SENSOR_PIN, INPUT_ANALOG);

  Serial.print("Setting up Hardware Timer for NTC Sensor with period: ");
  TIM_TypeDef *TempTimerInstance = TIM10;
  HardwareTimer *TempThread = new HardwareTimer(TempTimerInstance);
  TempThread->pause();
  TempThread->setPrescaleFactor(16384);
  Serial.print(TempThread->getOverflow() / (TempThread->getTimerClkFreq() / TempThread->getPrescaleFactor()));
  Serial.println(" sec");
  TempThread->refresh();
  TempThread->resume();
  TempThread->attachInterrupt(TemperatureSensorCallback);
}

void TemperatureSensorCallback()
{
  Serial.println("NTC sensor measure...");
  float Vout = analogRead(NTC_SENSOR_PIN)* (3.3 / STM32_ANALOG_RESOLUTION);
  float R_NTC = (Vout * NTC_REFERENCE_RESISTANCE) /(5.00 - Vout); 
  float temperature = ((NTC_NOMINAL_TEMPERATURE * NTC_B_VALUE)/(NTC_NOMINAL_TEMPERATURE * log(R_NTC / NTC_NOMINAL_RESISTANCE) + NTC_B_VALUE) - 273.15);
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
  Serial.print("Setting up Hardware Timer for TDS Sensor with period: ");
  TIM_TypeDef *TDSTimerInstance = TIM11;
  HardwareTimer *TDSThread = new HardwareTimer(TDSTimerInstance);
  TDSThread->pause();
  TDSThread->setPrescaleFactor(65536);
  Serial.print(TDSThread->getOverflow() / (TDSThread->getTimerClkFreq() / TDSThread->getPrescaleFactor()));
  Serial.println(" sec");
  TDSThread->refresh();
  TDSThread->resume();
  TDSThread->attachInterrupt(TDSSensorCallback);
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