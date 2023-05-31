#include <LiquidCrystal.h>
#include <Arduino.h>

#define LCR_PIN_RS PB4
#define LCD_PIN_ENABLE PB3
#define LCD_PIN_D4 PB15
#define LCD_PIN_D5 PB14
#define LCD_PIN_D6 PB13
#define LCD_PIN_D7 PB12

enum DisplayView {Temperature, TDS, Consumption, Filter1, Filter2, Filter3, Cancel, Done, Reset1, Reset2, Reset3};
extern int DisplayState;

void DisplayControlCallback();

void DisplayLoopCallback();

void initializeLCD(void);

void printLCD(int line, char *text);

void clearLCD(void);