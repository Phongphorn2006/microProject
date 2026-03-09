#pragma once
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;

void displayInit();
void displayUpdate(float currentFreq, float manualFreq, bool isAutoMode,
                   int animSpeed, int fanRpm, int fanPercent);
void lcdClear();
void lcdShowMessage(const char* msg);   // แสดง 800ms แล้ว clear
