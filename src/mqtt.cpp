#include "mqtt.h"
#include "strobe.h"   // isAutoMode, manualFreq, animSpeed, frameAdvanceMs
#include "fan.h"      // fanSoftStartTo, fanPercent

static WiFiClient  net;
static MQTTClient  mqttClient(256);
static unsigned long lastReconnectMs = 0;

// ── รับคำสั่งจาก broker ──────────────────────
static void onMessage(String& topic, String& payload) {
    if (topic == TOPIC_SUB_MODE) {
        if (payload == "AUTO")   { isAutoMode = true; }
        if (payload == "MANUAL") { isAutoMode = false; }
    }
    else if (topic == TOPIC_SUB_HZ) {
        float val = payload.toFloat();
        if (val >= 1.0f && val <= 250.0f) manualFreq = val;
        isAutoMode = false;
    }
    else if (topic == TOPIC_SUB_FAN) {
        int val = constrain(payload.toInt(), 0, 100);
        fanSoftStart(val, FAN_STEP_DELAY);   // soft start ทุกครั้งที่รับคำสั่งจาก MQTT
    }
    else if (topic == TOPIC_SUB_ANIM) {
        animSpeed      = constrain(payload.toInt(), 1, 20);
        frameAdvanceMs = (unsigned long)(animSpeed * 40);
    }
}

// ── เชื่อมต่อ / reconnect ────────────────────
static void mqttConnect() {
    if (millis() - lastReconnectMs < 5000) return;
    lastReconnectMs = millis();

    if (WiFi.status() != WL_CONNECTED) return;
    if (mqttClient.connected()) return;

    Serial.print("[MQTT] Connecting...");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
        Serial.println("connected");
        mqttClient.subscribe(TOPIC_SUB_MODE);
        mqttClient.subscribe(TOPIC_SUB_HZ);
        mqttClient.subscribe(TOPIC_SUB_FAN);
        mqttClient.subscribe(TOPIC_SUB_ANIM);
    } else {
        Serial.println("failed");
    }
}

// ── Public ───────────────────────────────────
void mqttInit() {
    mqttClient.begin(MQTT_BROKER, MQTT_PORT, net);
    mqttClient.onMessage(onMessage);
    mqttConnect();
}

void mqttUpdate() {
    mqttClient.loop();
    if (!mqttClient.connected()) mqttConnect();
}

void mqttPublish(float currentFreq, bool isAutoMode,
                 int fanRpm, int fanPercent, int animSpeed) {
    if (!mqttClient.connected()) return;
    mqttClient.publish(TOPIC_RPM,  String(fanRpm));
    mqttClient.publish(TOPIC_HZ,   String(currentFreq, 1));
    mqttClient.publish(TOPIC_MODE, isAutoMode ? "AUTO" : "MANUAL");
    mqttClient.publish(TOPIC_FAN,  String(fanPercent));
    Serial.println("Fan% =" + String(fanPercent));
}