#include <lcd.h>

LiquidCrystal lcd(LCR_PIN_RS, LCD_PIN_ENABLE, LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7);

void initializeLCD(void)
{
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("Initialising LCD....");
  delay(50);
  lcd.clear();
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