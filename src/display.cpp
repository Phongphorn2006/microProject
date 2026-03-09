#include "display.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

void displayInit() {
    lcd.init();
    lcd.backlight();
}

void lcdClear() {
    lcd.clear();
}

void lcdShowMessage(const char* msg) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(msg);
    delay(800);
    lcd.clear();
}

void displayUpdate(float currentFreq, float manualFreq, bool isAutoMode,
                   int animSpeed, int fanRpm, int fanPercent) {
    // Row 0: mode + freq
    lcd.setCursor(0, 0);
    if (isAutoMode) {
        lcd.print("AUTO ");
        lcd.print(currentFreq, 1);
        lcd.print("Hz S:");
        lcd.print(animSpeed);
        lcd.print("  ");
    } else {
        lcd.print("MANU ");
        lcd.print(manualFreq, 1);
        lcd.print("Hz      ");
    }

    // Row 1: RPM + fan%
    lcd.setCursor(0, 1);
    lcd.print("RPM:"); lcd.print(fanRpm);
    lcd.print(" Fan:"); lcd.print(fanPercent);
    lcd.print("%  ");
}
