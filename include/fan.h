#pragma once
#include <Arduino.h>
#include "config.h"

extern int fanPercent;

void fanInit();
void fanSetPercent(int percent);
int fanGetPWM(int percent);
