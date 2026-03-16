#include <LittleFS.h>
#include "config.h"
#include "fan.h"
#include "strobe.h"
#include "encoder.h"
#include "display.h"
#include "websocket.h"
#include "mqtt.h"

static unsigned long lastBroadcastMs = 0;
static float fanFreq = 0.0f;

void setup()
{

    Serial.begin(9600);

    // ── LittleFS ── mount ก่อนเสมอ
    if (!LittleFS.begin(true))
    {
        Serial.println("LittleFS FAILED");
    }
    else
    {
        Serial.println("LittleFS OK");
        // ลิสต์ไฟล์ทั้งหมดที่อยู่ใน LittleFS
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file)
        {
            Serial.print("FILE: ");
            Serial.println(file.name());
            file = root.openNextFile();
        }
    }

    fanInit();
    strobeInit();
    encoderInit();
    displayInit();

    mqttInit();

    lcd.setCursor(0, 0);
    lcd.print("Starting WiFi..");

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    IPAddress ip = WiFi.localIP();
    Serial.print("\nIP: ");
    Serial.println(ip);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(ip);
    delay(2000);
    lcd.clear();

    wsInit();
    strobeStartTask();
}

void loop()
{
    wsHandle();

    // คำนวณความถี่พัดลมจาก IR
    if (newPulse)
    {
        noInterrupts();
        unsigned long interval = irInterval;
        newPulse = false;
        interrupts();
        if (interval > 0)
        {
            float inst = 1000000.0f / (float)interval;
            fanFreq = (fanFreq == 0.0f) ? inst : (fanFreq * 0.85f + inst * 0.15f);
        }
    }
    if (micros() - lastIrTime > FAN_TIMEOUT_US)
        fanFreq = 0.0f;

    encoderUpdate();
    mqttUpdate();
    // Frame Advance
    if (isAutoMode && millis() - lastFrameAdvance >= frameAdvanceMs)
    {
        lastFrameAdvance = millis();
        currentFrame = (currentFrame + 1) % NUM_FRAMES;
    }

    // อัปเดต currentFreq ให้ StrobeTask
    portENTER_CRITICAL(&strobeMux);
    currentFreq = isAutoMode ? (fanFreq * NUM_FRAMES) : manualFreq;
    portEXIT_CRITICAL(&strobeMux);

    // LCD + WebSocket broadcast ทุก 500ms
    if (millis() - lastBroadcastMs > BROADCAST_INTERVAL)
    {
        lastBroadcastMs = millis();
        int fanRpm = (int)(fanFreq * 60.0f);
        displayUpdate(currentFreq, manualFreq, isAutoMode,
                      animSpeed, fanRpm, fanPercent);
        wsBroadcast(currentFreq, isAutoMode, fanRpm, fanPercent, animSpeed);
        mqttPublish(currentFreq, isAutoMode, fanRpm, fanPercent, animSpeed);
    }
}
