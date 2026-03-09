#pragma once
#include <Arduino.h>
#include "config.h"

// ตัวแปรที่ modules อื่นต้องอ่าน
extern volatile unsigned long lastIrTime;
extern volatile unsigned long irInterval;
extern volatile bool          newPulse;
extern volatile int           currentFrame;
extern volatile int           targetFrame;
extern volatile bool          firePending;
extern volatile float         currentFreq;
extern portMUX_TYPE           strobeMux;

extern bool          isAutoMode;
extern float         manualFreq;
extern int           animSpeed;
extern unsigned long frameAdvanceMs;
extern unsigned long lastFrameAdvance;

void strobeInit();
void strobeStartTask();
void IRAM_ATTR onIrPulse();
void StrobeTask(void* pvParameters);
