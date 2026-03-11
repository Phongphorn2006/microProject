#include "websocket.h"
#include "strobe.h"
#include "fan.h"
#include "display.h"

WebServer        httpServer(HTTP_PORT);
WebSocketsServer wsServer(WS_PORT);

// ── WebSocket Event Handler ───────────────────
static void onWsEvent(uint8_t clientNum, WStype_t type,
                      uint8_t* payload, size_t length) {
    if (type == WStype_CONNECTED) {
        Serial.printf("[WS] Client #%u connected\n", clientNum);
    } else if (type == WStype_DISCONNECTED) {
        Serial.printf("[WS] Client #%u disconnected\n", clientNum);
    } else if (type == WStype_TEXT) {
        StaticJsonDocument<128> doc;
        if (deserializeJson(doc, payload, length)) return;
        const char* cmd = doc["cmd"];
        if (!cmd) return;

        if (strcmp(cmd, "mode") == 0) {
            const char* val = doc["value"];
            if (val && strcmp(val, "AUTO")   == 0) { isAutoMode = true;  lcdClear(); }
            if (val && strcmp(val, "MANUAL") == 0) { isAutoMode = false; lcdClear(); }
        } else if (strcmp(cmd, "hz") == 0) {
            float val = doc["value"].as<float>();
            if (val >= 1.0f && val <= 250.0f) manualFreq = val;
            isAutoMode = false;
        } else if (strcmp(cmd, "fan") == 0) {
            fanSoftStart(doc["value"].as<int>(),FAN_STEP_DELAY);
        } else if (strcmp(cmd, "anim") == 0) {
            animSpeed      = constrain(doc["value"].as<int>(), 1, 20);
            frameAdvanceMs = (unsigned long)(animSpeed * 40);
        }
    }
}

// ── Public Functions ──────────────────────────
void wsInit() {
    // serve index.html จาก LittleFS

    // ต้อง Upload flash memory ก่อน
    // pio run --target uploadfs

    // ถ้าไม่ได้ให้ลบ flash memory
    // pio run -t erase     
    httpServer.on("/", HTTP_GET, []() {
        File f = LittleFS.open("/index.html", "r");
        if (!f) {
            httpServer.send(500, "text/plain", "LittleFS: index.html not found");
            return;
        }
        httpServer.streamFile(f, "text/html");
        f.close();
    });
    httpServer.begin();

    wsServer.begin();
    wsServer.onEvent(onWsEvent);
}

void wsHandle() {
    httpServer.handleClient();
    wsServer.loop();
}

void wsBroadcast(float currentFreq, bool isAutoMode,
                 int fanRpm, int fanPercent, int animSpeed) {
    StaticJsonDocument<128> doc;
    doc["rpm"]  = fanRpm;
    doc["hz"]   = String(currentFreq, 1);
    doc["mode"] = isAutoMode ? "AUTO" : "MANUAL";
    doc["fan"]  = fanPercent;
    doc["anim"] = animSpeed;
    String msg;
    serializeJson(doc, msg);
    wsServer.broadcastTXT(msg);
}