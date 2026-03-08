#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

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
// Wi-Fi (Station Mode — เชื่อมต่อ router เดิม)
// ==========================================
const char *WIFI_SSID = "@JumboPlusIoT";
const char *WIFI_PASS = "y3l5cfiq";

// ==========================================
// WebServer (port 80) + WebSocket (port 81)
// ==========================================
WebServer httpServer(80);
WebSocketsServer wsServer(81);

// ==========================================
// ตัวแปรระบบ
// ==========================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

volatile unsigned long lastIrTime = 0;
volatile unsigned long irInterval = 0;
volatile bool newPulse = false;

bool isAutoMode = true;
float manualFreq = 30.0f;
volatile float currentFreq = 0.0f;
int fanPercent = 0;

const int NUM_FRAMES = 7;

// ==========================================
// ตัวแปร Animation
// ==========================================
volatile int currentFrame = 0;
volatile int targetFrame = 0;
volatile bool firePending = false;

int animSpeed = 5;                  // 1(เร็ว) ~ 20(ช้า)
unsigned long frameAdvanceMs = 200; // = animSpeed * 40 ms/frame
unsigned long lastFrameAdvance = 0;

unsigned long strobeDuration = 100; // µs

int lastClk1 = HIGH;
int lastClk2 = HIGH;
unsigned long lastPublishMillis = 0;

portMUX_TYPE strobeMux = portMUX_INITIALIZER_UNLOCKED;

// ==========================================
// HTML Dashboard (เก็บใน Flash)
// ==========================================
// หน้าเว็บที่ ESP32 serve ให้ client เชื่อมต่อ WebSocket กลับมา port 81
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Zoetrope Control</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: 'Segoe UI', sans-serif; background: #0f0f1a; color: #e0e0ff; min-height: 100vh; padding: 20px; }
    h1 { text-align: center; font-size: 1.8rem; color: #a78bfa; margin-bottom: 20px; letter-spacing: 2px; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; max-width: 700px; margin: 0 auto; }
    .card { background: #1e1e2e; border-radius: 12px; padding: 18px; border: 1px solid #2d2d4e; }
    .card h2 { font-size: 0.85rem; color: #7c7caa; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 10px; }
    .value { font-size: 2.2rem; font-weight: bold; color: #a78bfa; }
    .unit  { font-size: 0.9rem; color: #7c7caa; margin-left: 4px; }
    .badge { display: inline-block; padding: 4px 14px; border-radius: 20px; font-size: 0.85rem; font-weight: bold; }
    .badge.auto   { background: #1a3a2a; color: #4ade80; border: 1px solid #4ade80; }
    .badge.manual { background: #3a1a1a; color: #f87171; border: 1px solid #f87171; }
    .ctrl-card { grid-column: 1 / -1; }
    .ctrl-row { display: flex; align-items: center; gap: 12px; margin-bottom: 14px; }
    .ctrl-row label { width: 130px; font-size: 0.9rem; color: #9999cc; flex-shrink: 0; }
    input[type=range] { flex: 1; accent-color: #a78bfa; }
    .ctrl-row span { width: 50px; text-align: right; font-size: 0.9rem; color: #e0e0ff; }
    .btn-row { display: flex; gap: 10px; margin-top: 6px; }
    button { flex: 1; padding: 10px; border: none; border-radius: 8px; cursor: pointer; font-size: 0.9rem; font-weight: bold; transition: opacity .2s; }
    button:hover { opacity: .85; }
    #btn-auto   { background: #4ade80; color: #0f1f15; }
    #btn-manual { background: #f87171; color: #1f0f0f; }
    .status-dot { width: 10px; height: 10px; border-radius: 50%; background: #4ade80; display: inline-block; margin-right: 6px; animation: blink 1s infinite; }
    @keyframes blink { 0%,100%{opacity:1} 50%{opacity:.3} }
    canvas { width: 100%; height: 80px; display: block; margin-top: 8px; }
  </style>
</head>
<body>
  <h1>⚡ Zoetrope Dashboard</h1>
  <div class="grid">
    <!-- Status cards -->
    <div class="card">
      <h2>RPM</h2>
      <span class="value" id="rpm">—</span><span class="unit">rpm</span>
    </div>
    <div class="card">
      <h2>Strobe Freq</h2>
      <span class="value" id="hz">—</span><span class="unit">Hz</span>
    </div>
    <div class="card">
      <h2>Mode</h2>
      <span id="mode-badge" class="badge auto">AUTO</span>
    </div>
    <div class="card">
      <h2>Fan Speed</h2>
      <span class="value" id="fan-val">—</span><span class="unit">%</span>
    </div>

    <!-- RPM Chart -->
    <div class="card ctrl-card">
      <h2><span class="status-dot"></span>RPM History</h2>
      <canvas id="chart"></canvas>
    </div>

    <!-- Controls -->
    <div class="card ctrl-card">
      <h2>Controls</h2>

      <div class="ctrl-row">
        <label>Manual Freq (Hz)</label>
        <input type="range" id="sl-hz" min="1" max="250" value="30" oninput="sendHz(this.value)">
        <span id="lbl-hz">30</span>
      </div>

      <div class="ctrl-row">
        <label>Fan Speed (%)</label>
        <input type="range" id="sl-fan" min="0" max="100" step="10" value="0" oninput="sendFan(this.value)">
        <span id="lbl-fan">0</span>
      </div>

      <div class="ctrl-row">
        <label>Anim Speed (1-20)</label>
        <input type="range" id="sl-anim" min="1" max="20" value="5" oninput="sendAnim(this.value)">
        <span id="lbl-anim">5</span>
      </div>

      <div class="btn-row">
        <button id="btn-auto"   onclick="sendMode('AUTO')">AUTO Mode</button>
        <button id="btn-manual" onclick="sendMode('MANUAL')">MANUAL Mode</button>
      </div>
    </div>
  </div>

<script>
  // ─── WebSocket ─────────────────────────────────
  // เชื่อมต่อกลับ ESP32 port 81
  const ws = new WebSocket('ws://' + location.hostname + ':81/');

  ws.onopen = () => console.log('WS connected');
  ws.onclose = () => setTimeout(() => location.reload(), 3000);

  ws.onmessage = (evt) => {
    try {
      const d = JSON.parse(evt.data);
      if (d.rpm !== undefined) {
        document.getElementById('rpm').textContent = d.rpm;
        pushChart(d.rpm);
      }
      if (d.hz !== undefined)   document.getElementById('hz').textContent  = parseFloat(d.hz).toFixed(1);
      if (d.mode !== undefined) {
        const badge = document.getElementById('mode-badge');
        badge.textContent = d.mode;
        badge.className = 'badge ' + d.mode.toLowerCase();
      }
      if (d.fan !== undefined) {
        document.getElementById('fan-val').textContent = d.fan;
        document.getElementById('sl-fan').value  = d.fan;
        document.getElementById('lbl-fan').textContent = d.fan;
      }
    } catch(e) {}
  };

  function send(obj) { if (ws.readyState === 1) ws.send(JSON.stringify(obj)); }
  function sendMode(m) { send({ cmd: 'mode', value: m }); }
  function sendHz(v)   { document.getElementById('lbl-hz').textContent = v;   send({ cmd: 'hz',   value: parseFloat(v) }); }
  function sendFan(v)  { document.getElementById('lbl-fan').textContent = v;  send({ cmd: 'fan',  value: parseInt(v) }); }
  function sendAnim(v) { document.getElementById('lbl-anim').textContent = v; send({ cmd: 'anim', value: parseInt(v) }); }

  // ─── Mini Chart ────────────────────────────────
  const canvas = document.getElementById('chart');
  const ctx = canvas.getContext('2d');
  const history = new Array(60).fill(0);

  function pushChart(val) {
    history.push(parseInt(val) || 0);
    if (history.length > 60) history.shift();
    drawChart();
  }

  function drawChart() {
    canvas.width  = canvas.offsetWidth;
    canvas.height = 80;
    const W = canvas.width, H = canvas.height;
    const max = Math.max(...history, 1);
    ctx.clearRect(0, 0, W, H);
    ctx.strokeStyle = '#a78bfa';
    ctx.lineWidth = 2;
    ctx.beginPath();
    history.forEach((v, i) => {
      const x = (i / (history.length - 1)) * W;
      const y = H - (v / max) * (H - 6) - 3;
      i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    });
    ctx.stroke();
  }
  drawChart();
</script>
</body>
</html>
)rawliteral";

// ==========================================
// 🔧 ฟังก์ชันแปลง % → PWM แบบ 3 โซน
// ==========================================
int getFanPWM(int percent)
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

// ==========================================
// Interrupt — IR Sensor
// ==========================================
void IRAM_ATTR onIrPulse()
{
  unsigned long now = micros();
  if (now - lastIrTime < 15000)
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
      delayMicroseconds(100);
      digitalWrite(STROBE_PIN, LOW);
    }
    else
    {
      portENTER_CRITICAL_ISR(&strobeMux);
      firePending = true;
      targetFrame = frame;
      portEXIT_CRITICAL_ISR(&strobeMux);
    }
  }
}

// ==========================================
// StrobeTask — Core 0
// ==========================================
TaskHandle_t StrobeTaskHandle;

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
          if (now - previousStrobeMicros > intervalMicros * 2)
            previousStrobeMicros = now;
          else
            previousStrobeMicros += intervalMicros;

          digitalWrite(STROBE_PIN, HIGH);
          delayMicroseconds(strobeDuration);
          digitalWrite(STROBE_PIN, LOW);
        }
        else
        {
          unsigned long timeToWait = intervalMicros - (now - previousStrobeMicros);
          if (timeToWait > 2000)
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
      unsigned long interval = 0;
      unsigned long irTime = 0;

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
        unsigned long frameDelay = (interval / NUM_FRAMES) * frame;
        unsigned long waitUntil = irTime + frameDelay;
        while (micros() < waitUntil)
        {
        }

        digitalWrite(STROBE_PIN, HIGH);
        delayMicroseconds(strobeDuration);
        digitalWrite(STROBE_PIN, LOW);
      }
      else
      {
        vTaskDelay(1);
      }
    }
  }
}

// ==========================================
// WebSocket Event Handler
// ==========================================
void 


(uint8_t clientNum, WStype_t type,
                      uint8_t *payload, size_t length)
{
  switch (type)
  {

  case WStype_CONNECTED:
    Serial.printf("[WS] Client #%u connected\n", clientNum);
    break;

  case WStype_DISCONNECTED:
    Serial.printf("[WS] Client #%u disconnected\n", clientNum);
    break;

  case WStype_TEXT:
  {
    // รับ JSON เช่น {"cmd":"mode","value":"AUTO"}
    // หรือ {"cmd":"hz","value":45.5}
    // หรือ {"cmd":"fan","value":50}
    // หรือ {"cmd":"anim","value":3}
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err)
      break;

    const char *cmd = doc["cmd"];
    if (!cmd)
      break;

    if (strcmp(cmd, "mode") == 0)
    {
      const char *val = doc["value"];
      if (val)
      {
        if (strcmp(val, "AUTO") == 0)
        {
          isAutoMode = true;
          lcd.clear();
        }
        if (strcmp(val, "MANUAL") == 0)
        {
          isAutoMode = false;
          lcd.clear();
        }
      }
    }
    else if (strcmp(cmd, "hz") == 0)
    {
      float val = doc["value"].as<float>();
      if (val >= 1.0f && val <= 250.0f)
        manualFreq = val;
      isAutoMode = false;
    }
    else if (strcmp(cmd, "fan") == 0)
    {
      fanPercent = constrain(doc["value"].as<int>(), 0, 100);
      ledcWrite(0, getFanPWM(fanPercent)); //ผิดบ่อย
    }
    else if (strcmp(cmd, "anim") == 0)
    {
      int spd = constrain(doc["value"].as<int>(), 1, 20);
      animSpeed = spd;
      frameAdvanceMs = (unsigned long)(animSpeed * 40);
    }
    break;
  }

  default:
    break;
  }
}

// ==========================================
// Setup
// ==========================================
void setup()
{
  Serial.begin(9600);

  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(STROBE_PIN, OUTPUT);
  digitalWrite(STROBE_PIN, LOW);

  pinMode(ENC1_CLK, INPUT);
  pinMode(ENC1_DT, INPUT);
  pinMode(ENC1_SW, INPUT_PULLUP);
  pinMode(ENC2_CLK, INPUT);
  pinMode(ENC2_DT, INPUT);
  pinMode(ENC2_SW, INPUT_PULLUP);

  // 1. ตั้งค่าคุณสมบัติ (Channel, Frequency, Resolution)
  ledcSetup(0, 5000, 8);
  // 2. ผูกพินเข้ากับ Channel ที่ตั้งไว้
  ledcAttachPin(FAN_PIN, 0);
  ledcWrite(0, 0);

  attachInterrupt(digitalPinToInterrupt(IR_PIN), onIrPulse, FALLING);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting WiFi..");

  // ── เชื่อมต่อ Wi-Fi router ──────────────────────
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
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

  // ── HTTP: serve หน้า Dashboard ──────────────────
  httpServer.on("/", HTTP_GET, []()
                { httpServer.send_P(200, "text/html", INDEX_HTML); });
  httpServer.begin();

  // ── WebSocket Server ────────────────────────────
  wsServer.begin();
  wsServer.onEvent(onWebSocketEvent);

  // ── Strobe Task บน Core 0 ───────────────────────
  xTaskCreatePinnedToCore(StrobeTask, "StrobeTask", 10000, NULL, 2,
                          &StrobeTaskHandle, 0);
}

// ==========================================
// Loop — Core 1
// ==========================================
void loop()
{
  httpServer.handleClient();
  wsServer.loop(); // แทน client.loop() ของ MQTT

  // ─────────────────────────────────────────
  // 1. คำนวณความถี่พัดลม (จาก IR)
  // ─────────────────────────────────────────
  static float fanFreq = 0.0f;
  if (newPulse)
  {
    noInterrupts();
    unsigned long interval = irInterval;
    newPulse = false;
    interrupts();

    if (interval > 0)
    {
      float instantFreq = 1000000.0f / (float)interval;
      fanFreq = (fanFreq == 0.0f) ? instantFreq
                                  : (fanFreq * 0.85f + instantFreq * 0.15f);
    }
  }
  if (micros() - lastIrTime > 500000)
    fanFreq = 0.0f;

  // ─────────────────────────────────────────
  // 2. ENC1 SW: กดสั้น = สลับ AUTO/MANUAL
  //             กดค้าง = reset manualFreq → 30 Hz
  // ─────────────────────────────────────────
  static int lastSw1State = HIGH;
  static unsigned long sw1PressTime = 0;
  int currentSw1State = digitalRead(ENC1_SW);

  if (currentSw1State == LOW && lastSw1State == HIGH)
  {
    sw1PressTime = millis();
  }
  else if (currentSw1State == HIGH && lastSw1State == LOW)
  {
    unsigned long dur = millis() - sw1PressTime;
    if (dur >= 800)
    {
      manualFreq = 30.0f;
      isAutoMode = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Reset: 30Hz");
      delay(800);
      lcd.clear();
    }
    else if (dur >= 50)
    {
      isAutoMode = !isAutoMode;
      lcd.clear();
    }
  }
  lastSw1State = currentSw1State;

  // ─────────────────────────────────────────
  // 3. ENC1 หมุน: ปรับ manualFreq (Manual mode)
  // ─────────────────────────────────────────
  int newClk1 = digitalRead(ENC1_CLK);
  if (newClk1 != lastClk1 && newClk1 == LOW)
  {
    manualFreq += (digitalRead(ENC1_DT) == newClk1) ? 0.5f : -0.5f;
    manualFreq = constrain(manualFreq, 1.0f, 250.0f);
    isAutoMode = false;
  }
  lastClk1 = newClk1;

  // ─────────────────────────────────────────
  // 4. ENC2 หมุน: ปรับความเร็ว Animation
  // ─────────────────────────────────────────
  int newClk2 = digitalRead(ENC2_CLK);
  if (newClk2 != lastClk2 && newClk2 == LOW)
  {
    if (digitalRead(ENC2_DT) != newClk2)
      animSpeed--;
    else
      animSpeed++;
    animSpeed = constrain(animSpeed, 1, 20);
    frameAdvanceMs = (unsigned long)(animSpeed * 40);
  }
  lastClk2 = newClk2;

  // ─────────────────────────────────────────
  // 5. ENC2 SW: กด = ปรับพัดลม +10% (วนรอบ)
  // ─────────────────────────────────────────
  static int lastSw2State = HIGH;
  static unsigned long sw2PressTime = 0;
  int currentSw2State = digitalRead(ENC2_SW);

  if (currentSw2State == LOW && lastSw2State == HIGH)
  {
    sw2PressTime = millis();
  }
  else if (currentSw2State == HIGH && lastSw2State == LOW)
  {
    if (millis() - sw2PressTime >= 50)
    {
      fanPercent = (fanPercent + 10) % 110;
      fanPercent = constrain(fanPercent, 0, 100);
      ledcWrite(0, getFanPWM(fanPercent));
    }
  }
  lastSw2State = currentSw2State;

  // ─────────────────────────────────────────
  // 6. Frame Advance
  // ─────────────────────────────────────────
  if (isAutoMode && (millis() - lastFrameAdvance >= frameAdvanceMs))
  {
    lastFrameAdvance = millis();
    currentFrame = (currentFrame + 1) % NUM_FRAMES;
  }

  // ─────────────────────────────────────────
  // 7. อัปเดต currentFreq ให้ StrobeTask
  // ─────────────────────────────────────────
  portENTER_CRITICAL(&strobeMux);
  currentFreq = isAutoMode ? (fanFreq * NUM_FRAMES) : manualFreq;
  portEXIT_CRITICAL(&strobeMux);

  // ─────────────────────────────────────────
  // 8. LCD + WebSocket Broadcast (ทุก 500ms)
  // ─────────────────────────────────────────
  if (millis() - lastPublishMillis > 500)
  {
    lastPublishMillis = millis();

    float fanRpm = fanFreq * 60.0f;

    // LCD Row 0
    lcd.setCursor(0, 0);
    if (isAutoMode)
    {
      lcd.print("AUTO ");
      lcd.print(currentFreq, 1);
      lcd.print("Hz S:");
      lcd.print(animSpeed);
      lcd.print("  ");
    }
    else
    {
      lcd.print("MANU ");
      lcd.print(manualFreq, 1);
      lcd.print("Hz      ");
    }

    // LCD Row 1
    lcd.setCursor(0, 1);
    lcd.print("RPM:");
    lcd.print((int)fanRpm);
    lcd.print(" Fan:");
    lcd.print(fanPercent);
    lcd.print("%  ");

    // ── WebSocket broadcast ── แทน MQTT publish ──
    // ส่ง JSON ออกหา clients ทุกตัวที่เชื่อมต่ออยู่
    StaticJsonDocument<128> doc;
    doc["rpm"] = (int)fanRpm;
    doc["hz"] = String(currentFreq, 1);
    doc["mode"] = isAutoMode ? "AUTO" : "MANUAL";
    doc["fan"] = fanPercent;
    doc["anim"] = animSpeed;

    String msg;
    serializeJson(doc, msg);
    wsServer.broadcastTXT(msg); // broadcast → clients ทุกตัว
  }
}
