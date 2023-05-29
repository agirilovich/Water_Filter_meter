#include "lcd.h"
#include "flowmeter.h"

int DisplayState = 0;

LiquidCrystal lcd(LCR_PIN_RS, LCD_PIN_ENABLE, LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7);

char DisplayBuf[16];

void initializeLCD(void)
{
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("Initializing LCD....");
  lcd.setCursor(0,1);
  lcd.print("Done");
  
}

void printLCD(int line, char *text)
{
    lcd.setCursor(0, line);
    lcd.print(text);
}

void clearLCD(void)
{
    lcd.clear();
}

void DisplayLoopCallback()
{
  if (DisplayState < 5)
  {
    DisplayState++;
  }
  else {
    DisplayState = 0;
  }
  
  DisplayControlCallback();
}

void DisplayControlCallback()
{
  char Title[16];
  switch(DisplayState)
  {
    case DisplayView::Temperature:
    {
      strcpy(Title, "Temperature, C:");
      sprintf(DisplayBuf, "%d", int(temperature));
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::TDS:
    {
      strcpy(Title, "Water TDS, ppm:");
      sprintf(DisplayBuf, "%d", int(tdsValue));
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::Consumption:
    {
      strcpy(Title, "Consumption, mL:");
      sprintf(DisplayBuf, "%d", int(ActualData.WaterConsumption));
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::Filter1:
    {
      strcpy(Title, "Filter 1, L:");
      sprintf(DisplayBuf, "%d", int(ActualData.WaterConsumptionFilter1 / 1000));
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::Filter2:
    {
      strcpy(Title, "Filter 2, L:");
      sprintf(DisplayBuf, "%d", int(ActualData.WaterConsumptionFilter2 / 1000));
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::Filter3:
    {
      strcpy(Title, "Filter 3, L:");
      sprintf(DisplayBuf, "%d", int(ActualData.WaterConsumptionFilter3 / 1000));
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::Reset1:
    {
      strcpy(Title, "Reset Filter 1?");
      strcpy(DisplayBuf, "NO          YES");
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::Reset2:
    {
      strcpy(Title, "Reset Filter 2?");
      strcpy(DisplayBuf, "NO          YES");
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::Reset3:
    {
      strcpy(Title, "Reset Filter 3?");
      strcpy(DisplayBuf, "NO          YES");
      clearLCD();
      printLCD(0, Title);
      printLCD(1, DisplayBuf);
      break;
    }
    case DisplayView::Cancel:
    {
      strcpy(Title, "Canceled");
      clearLCD();
      printLCD(0, Title);
      DisplayState = 0;
      break;
    }
    case DisplayView::Done:
    {
      strcpy(Title, "Done");
      clearLCD();
      printLCD(0, Title);
      DisplayState = 0;
      break;
    }
  }
}
