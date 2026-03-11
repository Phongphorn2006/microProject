#pragma once
#include <Arduino.h>
#include "config.h"

extern int fanPercent;

void fanInit();
void fanSetPercent(int percent);
int fanGetPWM(int percent);

void fanSoftStart(int targetPercent, int stepDelayMs = 10);
//                                                     ^--- default 10ms/step
//                                   ขึ้นจาก 0→100 ใช้เวลา 1000ms