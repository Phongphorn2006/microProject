#include "fan.h"

int fanPercent = 0;

void fanInit()
{
    ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_BITS);
    ledcAttachPin(FAN_PIN, FAN_PWM_CHANNEL);
    ledcWrite(0, 0);
}

// แปลง % → PWM แบบ 3 โซน (nonlinear mapping)
int fanGetPWM(int percent)
{
    if (percent <= 0)
        return 0;
    percent = constrain(percent, 0, 100);
    if (percent <= 20)
        return map(percent, 1, 20, 165, 185);
    else if (percent <= 60)
        return map(percent, 21, 60, 186, 220);
    else
        return map(percent, 61, 100, 221, 255);
}

void fanSetPercent(int percent)
{
    fanPercent = constrain(percent, 0, 100);
    ledcWrite(0, fanGetPWM(fanPercent));
}


void fanSoftStart(int targetPercent, int stepDelayMs) {
    targetPercent = constrain(targetPercent, 0, 100);

    // เริ่มจาก % ปัจจุบัน ค่อยๆ ขึ้นไปถึง target
    int start = fanPercent;

    if (start < targetPercent) {
        for (int p = start; p <= targetPercent; p++) {
            ledcWrite(0, fanGetPWM(p));
            delay(stepDelayMs);   // หน่วงแต่ละ step
        }
    } else {
        for (int p = start; p >= targetPercent; p--) {
            ledcWrite(0, fanGetPWM(p));
            delay(stepDelayMs);
        }
    }

    fanPercent = targetPercent;
}