#include "encoder.h"
#include "strobe.h"   // isAutoMode, manualFreq, animSpeed, frameAdvanceMs
#include "fan.h"      // fanSetPercent, fanPercent
#include "display.h"  // lcdClear, lcdPrint

static int lastClk1 = HIGH;
static int lastClk2 = HIGH;

static int lastSw1 = HIGH;
static unsigned long sw1PressTime = 0;

static int lastSw2 = HIGH;
static unsigned long sw2PressTime = 0;

void encoderInit() {
    pinMode(ENC1_CLK, INPUT); pinMode(ENC1_DT, INPUT);
    pinMode(ENC1_SW,  INPUT_PULLUP);
    pinMode(ENC2_CLK, INPUT); pinMode(ENC2_DT, INPUT);
    pinMode(ENC2_SW,  INPUT_PULLUP);
}

void encoderUpdate() {
    // ── ENC1 SW: กดสั้น = สลับ AUTO/MANUAL, กดค้าง = reset 30Hz ──
    int sw1 = digitalRead(ENC1_SW);
    if (sw1 == LOW  && lastSw1 == HIGH) sw1PressTime = millis();
    if (sw1 == HIGH && lastSw1 == LOW) {
        unsigned long dur = millis() - sw1PressTime;
        if (dur >= 800) {
            manualFreq = 30.0f;
            isAutoMode = false;
            lcdShowMessage("Reset: 30Hz");
        } else if (dur >= 50) {
            isAutoMode = !isAutoMode;
            lcdClear();
        }
    }
    lastSw1 = sw1;

    // ── ENC1 หมุน: ปรับ manualFreq ──
    int c1 = digitalRead(ENC1_CLK);
    if (c1 != lastClk1 && c1 == LOW) {
        manualFreq += (digitalRead(ENC1_DT) == c1) ? 0.5f : -0.5f;
        manualFreq  = constrain(manualFreq, 1.0f, 250.0f);
        isAutoMode  = false;
    }
    lastClk1 = c1;

    // ── ENC2 หมุน: ปรับ animSpeed ──
    int c2 = digitalRead(ENC2_CLK);
    if (c2 != lastClk2 && c2 == LOW) {
        if (digitalRead(ENC2_DT) != c2) animSpeed--;
        else                             animSpeed++;
        animSpeed      = constrain(animSpeed, 1, 20);
        frameAdvanceMs = (unsigned long)(animSpeed * 40);
    }
    lastClk2 = c2;

    // ── ENC2 SW: กด = ปรับพัดลม +10% ──
    int sw2 = digitalRead(ENC2_SW);
    if (sw2 == LOW  && lastSw2 == HIGH) sw2PressTime = millis();
    if (sw2 == HIGH && lastSw2 == LOW && millis() - sw2PressTime >= 50) {
        int next = constrain((fanPercent + 10) % 110, 0, 100);
        fanSetPercent(next);
    }
    lastSw2 = sw2;
}
