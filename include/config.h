#pragma once

// ==========================================
// Pin Mapping
// ==========================================
#define IR_PIN 23
#define STROBE_PIN 19
#define FAN_PIN 5

#define ENC1_CLK 25
#define ENC1_DT 26
#define ENC1_SW 27

#define ENC2_CLK 32
#define ENC2_DT 33
#define ENC2_SW 35

// ==========================================
// Wi-Fi
// ==========================================
#define WIFI_SSID "@JumboPlusIoT"
#define WIFI_PASS "y3l5cfiq"

// ==========================================
// WebServer / WebSocket ports
// ==========================================
#define HTTP_PORT 80
#define WS_PORT 81

// ==========================================
// System Constants
// ==========================================
#define NUM_FRAMES 7
#define STROBE_DURATION_US 100 // µs
#define IR_DEBOUNCE_US 15000   // µs
#define FAN_PWM_CHANNEL 0
#define FAN_PWM_FREQ 5000 // Hz
#define FAN_PWM_BITS 8
#define BROADCAST_INTERVAL 500 // ms
#define FAN_TIMEOUT_US 500000  // µs → หยุดหมุน
