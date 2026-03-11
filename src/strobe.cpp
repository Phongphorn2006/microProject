#include "strobe.h"

volatile unsigned long lastIrTime = 0;
volatile unsigned long irInterval = 0;
volatile bool newPulse = false;
volatile int currentFrame = 0;
volatile int targetFrame = 0;
volatile bool firePending = false;
volatile float currentFreq = 0.0f;
portMUX_TYPE strobeMux = portMUX_INITIALIZER_UNLOCKED;

bool isAutoMode = true;
float manualFreq = 30.0f;
int animSpeed = 5;
unsigned long frameAdvanceMs = 200;
unsigned long lastFrameAdvance = 0;

static TaskHandle_t StrobeTaskHandle = nullptr;

void strobeInit()
{
    pinMode(STROBE_PIN, OUTPUT);
    digitalWrite(STROBE_PIN, LOW);
    pinMode(IR_PIN, INPUT_PULLUP);
}

void strobeStartTask()
{
    attachInterrupt(digitalPinToInterrupt(IR_PIN), onIrPulse, FALLING);
    xTaskCreatePinnedToCore(
        StrobeTask,   // ฟังก์ชันที่จะรัน
        "StrobeTask", // ชื่อ task
        10000,        // stack size (bytes)
        NULL,         // parameter
        2,            // priority (สูงกว่า loop = 1)
        &StrobeTaskHandle,
        0 // Core 0
    );
    // loop() รันบน Core 1 โดย Arduino framework อัตโนมัติ
}

// ── IR Interrupt ─────────────────────────────
// ISR — ทำงานทันทีเมื่อ IR ตรวจจับใบพัด
void IRAM_ATTR onIrPulse()
{
    unsigned long now = micros();
    if (now - lastIrTime < IR_DEBOUNCE_US)
        return;
    irInterval = now - lastIrTime;
    lastIrTime = now;
    newPulse = true;
    if (isAutoMode)
    {
        int frame = currentFrame;
        unsigned long frameDelay = (irInterval / NUM_FRAMES) * frame;

        if (frameDelay < 100)
        {
            digitalWrite(STROBE_PIN, HIGH);
            delayMicroseconds(STROBE_DURATION_US);
            digitalWrite(STROBE_PIN, LOW);
        }
        else
        {
            // ส่งสัญญาณให้ StrobeTask บน Core 0
            portENTER_CRITICAL_ISR(&strobeMux);
            firePending = true;
            targetFrame = frame;
            portEXIT_CRITICAL_ISR(&strobeMux);
        }
    }
}

// ── Strobe Task — Core 0 ──────────────────────
void StrobeTask(void *pvParameters)
{
    static unsigned long previousStrobeMicros = 0;

    for (;;)
    {
        if (!isAutoMode)
        {
            portENTER_CRITICAL(&strobeMux);
            float freq = currentFreq;
            portEXIT_CRITICAL(&strobeMux);

            if (freq > 1.0f)
            {
                unsigned long now = micros();
                unsigned long intervalMicros = (unsigned long)(1000000.0f / freq);

                if (now - previousStrobeMicros >= intervalMicros)
                {
                    previousStrobeMicros = (now - previousStrobeMicros > intervalMicros * 2)
                                               ? now
                                               : previousStrobeMicros + intervalMicros;
                    digitalWrite(STROBE_PIN, HIGH);
                    delayMicroseconds(STROBE_DURATION_US);
                    digitalWrite(STROBE_PIN, LOW);
                }
                else
                {
                    if (intervalMicros - (now - previousStrobeMicros) > 2000)
                        vTaskDelay(1);
                }
            }
            else
            {
                vTaskDelay(10);
            }
        }
        else
        {
            bool doFire = false;
            int frame = 0;
            unsigned long interval = 0, irTime = 0;

            portENTER_CRITICAL(&strobeMux);
            if (firePending)
            {
                doFire = true;
                firePending = false;
                frame = targetFrame;
                interval = irInterval;
                irTime = lastIrTime;
            }
            portEXIT_CRITICAL(&strobeMux);

            if (doFire && interval > 0)
            {

                unsigned long waitUntil = irTime + (interval / NUM_FRAMES) * frame;
                while (micros() < waitUntil)
                {
                } // busy-wait แม่นยำ
                digitalWrite(STROBE_PIN, HIGH);
                delayMicroseconds(STROBE_DURATION_US);
                digitalWrite(STROBE_PIN, LOW);
            }
            else
            {
                vTaskDelay(1);
            }
        }
    }
}
