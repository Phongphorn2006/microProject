#include "websocket.h"
#include "strobe.h"  // isAutoMode, manualFreq, animSpeed, frameAdvanceMs
#include "fan.h"     // fanSetPercent, fanPercent
#include "display.h" // lcdClear

WebServer httpServer(HTTP_PORT);
WebSocketsServer wsServer(WS_PORT);

// ==========================================
// HTML Dashboard (เก็บใน Flash)
// ==========================================
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
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
    <div class="card"><h2>RPM</h2><span class="value" id="rpm">—</span><span class="unit">rpm</span></div>
    <div class="card"><h2>Strobe Freq</h2><span class="value" id="hz">—</span><span class="unit">Hz</span></div>
    <div class="card"><h2>Mode</h2><span id="mode-badge" class="badge auto">AUTO</span></div>
    <div class="card"><h2>Fan Speed</h2><span class="value" id="fan-val">—</span><span class="unit">%</span></div>
    <div class="card ctrl-card">
      <h2><span class="status-dot"></span>RPM History</h2>
      <canvas id="chart"></canvas>
    </div>
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
  const ws = new WebSocket('ws://' + location.hostname + ':81/');
  ws.onopen  = () => console.log('WS connected');
  ws.onclose = () => setTimeout(() => location.reload(), 3000);
  ws.onmessage = (evt) => {
    try {
      const d = JSON.parse(evt.data);
      if (d.rpm  !== undefined) { document.getElementById('rpm').textContent = d.rpm; pushChart(d.rpm); }
      if (d.hz   !== undefined)   document.getElementById('hz').textContent  = parseFloat(d.hz).toFixed(1);
      if (d.mode !== undefined) {
        const b = document.getElementById('mode-badge');
        b.textContent = d.mode; b.className = 'badge ' + d.mode.toLowerCase();
      }
      if (d.fan !== undefined) {
        document.getElementById('fan-val').textContent = d.fan;
        document.getElementById('sl-fan').value = d.fan;
        document.getElementById('lbl-fan').textContent = d.fan;
      }
    } catch(e) {}
  };
  function send(o) { if (ws.readyState===1) ws.send(JSON.stringify(o)); }
  function sendMode(m) { send({cmd:'mode', value:m}); }
  function sendHz(v)   { document.getElementById('lbl-hz').textContent=v;   send({cmd:'hz',   value:parseFloat(v)}); }
  function sendFan(v)  { document.getElementById('lbl-fan').textContent=v;  send({cmd:'fan',  value:parseInt(v)}); }
  function sendAnim(v) { document.getElementById('lbl-anim').textContent=v; send({cmd:'anim', value:parseInt(v)}); }

  const canvas=document.getElementById('chart'), ctx=canvas.getContext('2d');
  const history=new Array(60).fill(0);
  function pushChart(val){ history.push(parseInt(val)||0); if(history.length>60)history.shift(); drawChart(); }
  function drawChart(){
    canvas.width=canvas.offsetWidth; canvas.height=80;
    const W=canvas.width, H=canvas.height, max=Math.max(...history,1);
    ctx.clearRect(0,0,W,H); ctx.strokeStyle='#a78bfa'; ctx.lineWidth=2; ctx.beginPath();
    history.forEach((v,i)=>{ const x=(i/(history.length-1))*W, y=H-(v/max)*(H-6)-3; i===0?ctx.moveTo(x,y):ctx.lineTo(x,y); });
    ctx.stroke();
  }
  drawChart();
</script>
</body>
</html>
)rawliteral";

// ── WebSocket Event Handler ───────────────────
static void onWsEvent(uint8_t clientNum, WStype_t type,
                      uint8_t *payload, size_t length)
{
    if (type == WStype_CONNECTED)
    {
        Serial.printf("[WS] Client #%u connected\n", clientNum);
    }
    else if (type == WStype_DISCONNECTED)
    {
        Serial.printf("[WS] Client #%u disconnected\n", clientNum);
    }
    else if (type == WStype_TEXT)
    {
        StaticJsonDocument<128> doc;
        if (deserializeJson(doc, payload, length))
            return;
        const char *cmd = doc["cmd"];
        if (!cmd)
            return;

        if (strcmp(cmd, "mode") == 0)
        {
            const char *val = doc["value"];
            if (val && strcmp(val, "AUTO") == 0)
            {
                isAutoMode = true;
                lcdClear();
            }
            if (val && strcmp(val, "MANUAL") == 0)
            {
                isAutoMode = false;
                lcdClear();
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
            fanSetPercent(doc["value"].as<int>());
        }
        else if (strcmp(cmd, "anim") == 0)
        {
            animSpeed = constrain(doc["value"].as<int>(), 1, 20);
            frameAdvanceMs = (unsigned long)(animSpeed * 40);
        }
    }
}

// ── Public Functions ──────────────────────────
void wsInit()
{
    httpServer.on("/", HTTP_GET, []()
                  { httpServer.send_P(200, "text/html", INDEX_HTML); });
    httpServer.begin();

    wsServer.begin();
    wsServer.onEvent(onWsEvent);
}

void wsHandle()
{
    httpServer.handleClient();
    wsServer.loop();
}

void wsBroadcast(float currentFreq, bool isAutoMode,
                 int fanRpm, int fanPercent, int animSpeed)
{
    StaticJsonDocument<128> doc;
    doc["rpm"] = fanRpm;
    doc["hz"] = String(currentFreq, 1);
    doc["mode"] = isAutoMode ? "AUTO" : "MANUAL";
    doc["fan"] = fanPercent;
    doc["anim"] = animSpeed;
    String msg;
    serializeJson(doc, msg);
    wsServer.broadcastTXT(msg);
}
