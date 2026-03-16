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

// ── IR Interrupt ────────────────
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

            // อ่าน currentFreq อย่างปลอดภัยจาก Core 1
            portENTER_CRITICAL(&strobeMux);
            float freq = currentFreq;
            portEXIT_CRITICAL(&strobeMux);

            if (freq > 1.0f)
            {
                unsigned long now = micros();
                unsigned long intervalMicros = (unsigned long)(1000000.0f / freq);
                // เช่น freq = 30Hz → interval = 33,333 µs

                if (now - previousStrobeMicros >= intervalMicros)
                {

                    // ถ้าเลยกำหนดไปมากกว่า 2 เท่า (เช่น task ถูกบล็อกนาน)
                    // reset เวลาใหม่เลย แทนที่จะยิงชดเชยหลายครั้ง
                    previousStrobeMicros = (now - previousStrobeMicros > intervalMicros * 2)
                                               ? now                                    // reset
                                               : previousStrobeMicros + intervalMicros; // เลื่อน 1 interval

                    // ยิง strobe
                    digitalWrite(STROBE_PIN, HIGH);
                    delayMicroseconds(STROBE_DURATION_US); // 100 µs
                    digitalWrite(STROBE_PIN, LOW);
                }
                else
                {
                    // ยังไม่ถึงเวลา — ถ้าเหลืออีก > 2ms ให้คืน CPU แทนที่จะ busy-wait
                    // ป้องกัน Core 0 กิน CPU 100% โดยเปล่าประโยชน์
                    if (intervalMicros - (now - previousStrobeMicros) > 2000)
                        vTaskDelay(1); // หยุด 1 tick (~1ms) แล้วกลับมาเช็คใหม่
                }
            }
            else
            {
                vTaskDelay(10); // freq ต่ำเกินไป ไม่ยิง รอ 10ms
            }
        }
        else
        {
            // รับข้อมูลจาก ISR ที่ส่งมาผ่านตัวแปร shared
            bool doFire = false;
            int frame = 0;
            unsigned long interval = 0, irTime = 0;

            // อ่านและล้าง firePending ในครั้งเดียว ป้องกัน race condition
            portENTER_CRITICAL(&strobeMux);
            if (firePending)
            {
                doFire = true;
                firePending = false;   // ล้างทันที ป้องกันยิงซ้ำ
                frame = targetFrame;   // frame ที่ ISR ส่งมา (0–6)
                interval = irInterval; // ระยะเวลา 1 รอบพัดลม (µs)
                irTime = lastIrTime;   // เวลาที่ IR pulse เกิดขึ้น
            }
            portEXIT_CRITICAL(&strobeMux);

            if (doFire && interval > 0)
            {
                // คำนวณเวลาที่ต้องยิง
                // แบ่ง 1 รอบเป็น NUM_FRAMES ช่วง แล้วเลือกช่วงของ frame นั้น
                // เช่น interval=10,000µs, frame=3, NUM_FRAMES=7
                // → waitUntil = irTime + (10000/7)*3 = irTime + 4,285µs
                unsigned long waitUntil = irTime + (interval / NUM_FRAMES) * frame;

                // busy-wait แม่นยำระดับ µs
                // ไม่ใช้ delay() เพราะ resolution ไม่พอ
                while (micros() < waitUntil)
                {
                }

                // ยิง strobe ตรงตำแหน่ง frame
                digitalWrite(STROBE_PIN, HIGH);
                delayMicroseconds(STROBE_DURATION_US); // 100 µs
                digitalWrite(STROBE_PIN, LOW);
            }
            else
            {
                // ยังไม่มี pulse ใหม่จาก IR → คืน CPU รอ 1 tick
                vTaskDelay(1);
            }
        }
    }
}
