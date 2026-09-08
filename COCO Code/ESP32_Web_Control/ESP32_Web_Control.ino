#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <Update.h>

// -----------------------------------------------------------------------------
// ESP32 Wi-Fi controller for the COCO spider robot
//
// This sketch uses an ESP32 web page to provide touch controls, forwarding
// the commands over Serial2 (pins 16/17) to the robot controller.
//
// Commands:
//   F = forward
//   B = back
//   L = left
//   R = right
//   S = stop
//   P = basic position
//   Q = spider position
//   U = hand shake
//   W = hand wave
//   V = body dance
//   O = LED on
//   X = LED off
//   K = LED blink
// -----------------------------------------------------------------------------

const char* apSsid = "SpiderRobot";
const char* apPassword = "12345678";

// ESP32 UART2 pins. Change these if your wiring uses different pins.
static const int ESP32_RX_PIN = 16;
static const int ESP32_TX_PIN = 17;
static const uint32_t ROBOT_BAUD = 9600;

IPAddress apIP(192, 168, 4, 1);
IPAddress apGateway(192, 168, 4, 1);
IPAddress apSubnet(255, 255, 255, 0);

WebServer server(80);

const char updateHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Firmware Update</title>
  <style>
    body { background: #020611; color: #00f3ff; font-family: monospace; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }
    .box { background: rgba(6, 15, 30, 0.6); padding: 30px; border: 1px solid rgba(0, 243, 255, 0.2); border-radius: 8px; box-shadow: 0 0 15px rgba(0, 243, 255, 0.05); text-align: center; }
    h2 { margin-top: 0; letter-spacing: 2px; }
    input[type=file] { margin: 20px 0; color: #5e849c; }
    input[type=submit] { background: rgba(0, 0, 0, 0.5); border: 1px solid var(--neon-cyan); color: #00f3ff; padding: 10px 20px; cursor: pointer; transition: 0.2s; border-color: rgba(0, 243, 255, 0.5); }
    input[type=submit]:hover { background: rgba(0, 243, 255, 0.1); box-shadow: 0 0 10px rgba(0, 243, 255, 0.2); }
    a { color: #5e849c; text-decoration: none; margin-top: 20px; display: inline-block; font-size: 0.8rem; }
    a:hover { color: #00f3ff; }
  </style>
</head>
<body>
  <div class="box">
    <h2>[ FIRMWARE UPDATE ]</h2>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update" accept=".bin" required><br>
      <input type="submit" value="UPLOAD & FLASH">
    </form>
    <a href="/">&lt; RETURN TO CONTROL</a>
  </div>
</body>
</html>
)rawliteral";

const char indexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Spider Tactical UI</title>
  <style>
    :root {
      --bg-base: #020611;
      --bg-glow: #0a1931;
      --neon-cyan: #00f3ff;
      --neon-red: #ff003c;
      --neon-green: #39ff14;
      --panel-bg: rgba(6, 15, 30, 0.6);
      --panel-border: rgba(0, 243, 255, 0.2);
      --text-main: #e0f8ff;
      --text-muted: #5e849c;
    }
    * { box-sizing: border-box; }
    html, body {
      -webkit-touch-callout: none;
      -webkit-user-select: none;
      user-select: none;
      height: 100%;
      margin: 0;
      font-family: 'Segoe UI', 'Roboto', 'Space Grotesk', monospace;
      background: var(--bg-base);
    }
    body {
      background-image: 
        radial-gradient(circle at 50% 50%, var(--bg-glow) 0%, var(--bg-base) 80%),
        linear-gradient(0deg, rgba(0,243,255,0.03) 1px, transparent 1px),
        linear-gradient(90deg, rgba(0,243,255,0.03) 1px, transparent 1px);
      background-size: 100% 100%, 30px 30px, 30px 30px;
      color: var(--text-main);
      display: flex;
      flex-direction: column;
      padding: 10px 20px;
      overflow-x: hidden;
    }
    .neon-box {
      background: var(--panel-bg);
      border: 1px solid var(--panel-border);
      box-shadow: 0 0 15px rgba(0, 243, 255, 0.05), inset 0 0 20px rgba(0, 243, 255, 0.05);
      border-radius: 8px;
      backdrop-filter: blur(10px);
      position: relative;
    }
    .neon-box::after {
      content: ''; position: absolute; top: 0; left: 0; right: 0; height: 1px;
      background: linear-gradient(90deg, transparent, var(--neon-cyan), transparent);
      opacity: 0.5;
    }
    .header {
      display: flex; justify-content: space-between; align-items: center;
      padding: 15px 20px; margin-bottom: 20px;
    }
    .logo {
      font-size: 1.5rem; font-weight: 900; letter-spacing: 2px;
      color: var(--neon-cyan); text-shadow: 0 0 10px rgba(0, 243, 255, 0.5);
      display: flex; align-items: center; gap: 10px;
    }
    .logo-icon { font-size: 2rem; }
    .header-status {
      display: flex; align-items: center; gap: 15px; font-size: 0.85rem;
      color: var(--neon-cyan); letter-spacing: 1px;
    }
    .dot {
      display: inline-block; width: 8px; height: 8px; border-radius: 50%;
      background: var(--neon-green); box-shadow: 0 0 10px var(--neon-green);
      animation: pulse 2s infinite; margin-right: 5px;
    }
    @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.4; } 100% { opacity: 1; } }
    .main-grid {
      display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 20px; flex: 1; margin-bottom: 20px;
    }
    .panel { padding: 20px; display: flex; flex-direction: column; }
    h2.panel-title {
      font-size: 0.95rem; color: var(--neon-cyan); letter-spacing: 3px;
      margin-top: 0; margin-bottom: 20px; border-bottom: 1px solid rgba(0, 243, 255, 0.2);
      padding-bottom: 10px; text-transform: uppercase;
    }
    .bracket { color: var(--text-muted); }
    /* Movement panel */
    .dpad-container { flex: 1; display: flex; flex-direction: column; justify-content: center; align-items: center; }
    .dpad {
      display: grid; grid-template-columns: repeat(3, 80px); grid-template-rows: repeat(3, 80px);
      gap: 15px; margin-bottom: 20px;
    }
    .dpad-btn {
      background: rgba(0, 243, 255, 0.05); border: 1px solid var(--neon-cyan);
      color: var(--neon-cyan); box-shadow: inset 0 0 15px rgba(0, 243, 255, 0.1);
      border-radius: 12px; font-size: 1.5rem; font-weight: bold; cursor: pointer;
      display: flex; justify-content: center; align-items: center;
      transition: all 0.1s ease; touch-action: none;
    }
    .dpad-btn:hover { background: rgba(0, 243, 255, 0.15); box-shadow: 0 0 20px rgba(0, 243, 255, 0.4); }
    .dpad-btn:active { background: var(--neon-cyan); color: #000; transform: scale(0.95); }
    .dpad-up { grid-column: 2; grid-row: 1; }
    .dpad-left { grid-column: 1; grid-row: 2; }
    .dpad-right { grid-column: 3; grid-row: 2; }
    .dpad-down { grid-column: 2; grid-row: 3; }
    .dpad-center {
      grid-column: 2; grid-row: 2; border-color: var(--neon-red); color: var(--neon-red);
      font-size: 0.8rem; box-shadow: inset 0 0 15px rgba(255, 0, 60, 0.1); font-weight: 900; letter-spacing: 1px;
    }
    .dpad-center:hover { background: rgba(255, 0, 60, 0.15); box-shadow: 0 0 20px rgba(255, 0, 60, 0.4); }
    .dpad-center:active { background: var(--neon-red); color: #fff; }
    .instruction { color: var(--text-muted); font-size: 0.75rem; letter-spacing: 2px; text-align: center; }
    
    /* Center status */
    .status-list { display: flex; flex-direction: column; gap: 15px; font-size: 0.9rem; letter-spacing: 1px; flex: 1; }
    .status-item {
      display: flex; justify-content: space-between; align-items: center;
      background: rgba(0, 0, 0, 0.4); padding: 12px 15px; border-radius: 6px;
      border: 1px solid rgba(255, 255, 255, 0.05); border-left: 3px solid var(--neon-cyan);
    }
    .status-label { color: var(--text-muted); }
    .status-val { color: var(--neon-cyan); font-weight: bold; text-shadow: 0 0 5px rgba(0, 243, 255, 0.3); }
    .status-val.alert { color: var(--neon-red); text-shadow: 0 0 5px rgba(255, 0, 60, 0.3); }
    .center-visual {
      flex: 1; display: flex; justify-content: center; align-items: center; min-height: 150px; position: relative;
    }
    .core-reactor {
      width: 100px; height: 100px; border-radius: 50%;
      border: 2px dashed var(--neon-cyan); animation: spin 20s linear infinite;
      display: flex; justify-content: center; align-items: center;
      box-shadow: 0 0 30px rgba(0, 243, 255, 0.1);
    }
    .core-inner {
      width: 60px; height: 60px; border-radius: 50%;
      background: radial-gradient(circle, var(--neon-cyan) 0%, transparent 70%);
      opacity: 0.5; animation: pulse 3s infinite;
    }
    @keyframes spin { 100% { transform: rotate(360deg); } }
    
    /* Action right panel */
    .action-group { margin-bottom: 20px; }
    .action-group h3 { font-size: 0.75rem; color: var(--text-muted); letter-spacing: 2px; margin: 0 0 10px 0; }
    .btn-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .action-btn {
      background: rgba(0, 0, 0, 0.5); border: 1px solid rgba(255, 255, 255, 0.15);
      color: var(--text-main); padding: 12px 10px; font-size: 0.8rem; letter-spacing: 1px;
      border-radius: 6px; cursor: pointer; transition: all 0.2s;
      text-transform: uppercase; position: relative; overflow: hidden;
      display: flex; align-items: center; gap: 8px; justify-content: center;
    }
    .action-btn::before { content: ''; position: absolute; left: 0; top: 0; bottom: 0; width: 3px; background: var(--neon-cyan); opacity: 0.3; transition: 0.2s; }
    .action-btn:hover { border-color: var(--neon-cyan); background: rgba(0, 243, 255, 0.1); box-shadow: 0 0 10px rgba(0, 243, 255, 0.2); }
    .action-btn:hover::before { opacity: 1; box-shadow: 0 0 8px var(--neon-cyan); }
    .action-btn:active { transform: scale(0.97); }
    
    .action-btn.toggle { border-color: rgba(255, 0, 60, 0.3); }
    .action-btn.toggle::before { background: var(--neon-red); opacity: 0.5; }
    .action-btn.toggle:hover { border-color: var(--neon-red); background: rgba(255, 0, 60, 0.1); box-shadow: 0 0 10px rgba(255, 0, 60, 0.2); }
    
    .action-btn.toggle.on { border-color: var(--neon-green); color: #fff; text-shadow: 0 0 5px var(--neon-green); background: rgba(57, 255, 20, 0.1); }
    .action-btn.toggle.on::before { background: var(--neon-green); opacity: 1; box-shadow: 0 0 8px var(--neon-green); }
    
    .action-btn.toggle.blink { border-color: orange; color: #fff; text-shadow: 0 0 5px orange; background: rgba(255, 165, 0, 0.1); }
    .action-btn.toggle.blink::before { background: orange; opacity: 1; box-shadow: 0 0 8px orange; }

    /* Footer */
    .footer {
      display: flex; justify-content: space-between; border-top: 1px dashed rgba(0, 243, 255, 0.3);
      padding-top: 15px; font-size: 0.75rem; color: var(--text-muted); letter-spacing: 2px;
    }
    .footer-highlight { color: var(--neon-cyan); }

    /* Media query for smaller mobile */
    @media (max-width: 768px) {
      .header { flex-direction: column; gap: 15px; }
      .footer { flex-direction: column; text-align: center; gap: 10px; }
      .dpad { grid-template-columns: repeat(3, 70px); grid-template-rows: repeat(3, 70px); gap: 10px;}
      .dpad-btn { font-size: 1.2rem; }
      .btn-grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <header class="header neon-box">
    <div class="logo">
      <span class="logo-icon">&#128375;</span> 
      <div>
        SPIDER CONTROLLER<br>
        <span style="font-size:0.6rem; color:var(--text-muted); letter-spacing:4px; font-weight:normal;">TACTICAL INTERFACE V2.0</span>
      </div>
    </div>
    <div class="header-status">
      <div class="status-badge"><span class="dot"></span>WIFI DIRECT</div>
      <div style="background: rgba(0,243,255,0.1); padding: 5px 10px; border: 1px solid var(--neon-cyan); border-radius: 4px;">AP: SpiderRobot</div>
    </div>
  </header>

  <main class="main-grid">
    <section class="panel neon-box">
      <h2 class="panel-title"><span class="bracket">[</span> MOVEMENT <span class="bracket">]</span></h2>
      <div class="dpad-container">
        <div class="dpad">
          <button class="dpad-btn dpad-up hold" data-cmd="F">&#9650;</button>
          <button class="dpad-btn dpad-left hold" data-cmd="L">&#9664;</button>
          <button class="dpad-btn dpad-center hold" data-cmd="S">STOP</button>
          <button class="dpad-btn dpad-right hold" data-cmd="R">&#9654;</button>
          <button class="dpad-btn dpad-down hold" data-cmd="B">&#9660;</button>
        </div>
        <div class="instruction">PRESS &amp; HOLD DIRECTION TO MOVE<br>RELEASE TO HALT</div>
      </div>
    </section>

    <section class="panel neon-box center-panel">
      <h2 class="panel-title"><span class="bracket">[</span> SYSTEM STATUS <span class="bracket">]</span></h2>
      <div class="status-list">
        <div class="status-item">
          <span class="status-label">LINK STATE</span>
          <span class="status-val" style="color:var(--neon-green)">SECURE</span>
        </div>
        <div class="status-item">
          <span class="status-label">TARGET ESP32</span>
          <span class="status-val">192.168.4.1</span>
        </div>
        <div class="status-item">
          <span class="status-label">ACTIVE OPCODE</span>
          <span class="status-val" id="currentCmd">NONE</span>
        </div>
        <div class="status-item">
          <span class="status-label">TELEMETRY</span>
          <span class="status-val alert" id="currentMsg">STANDBY</span>
        </div>
      </div>
      <div class="center-visual">
         <div class="core-reactor">
           <div class="core-inner"></div>
         </div>
         <!-- Hidden status blocks to keep compatible with previous JS ids -->
         <span id="cmd" style="display:none">none</span>
         <span id="msg" style="display:none">Ready</span>
      </div>
    </section>

    <section class="panel neon-box">
      <h2 class="panel-title"><span class="bracket">[</span> ACTIONS &amp; PROTOCOLS <span class="bracket">]</span></h2>
      
      <div class="action-group">
        <h3>POSES</h3>
        <div class="btn-grid">
          <button class="action-btn" onclick="sendCmd('P')">BASIC POS</button>
          <button class="action-btn" onclick="sendCmd('Q')">SPIDER POS</button>
        </div>
      </div>

      <div class="action-group">
        <h3>GESTURES</h3>
        <div class="btn-grid" style="grid-template-columns: 1fr;">
          <button class="action-btn" onclick="sendCmd('U')">HAND SHAKE</button>
          <button class="action-btn" onclick="sendCmd('W')">HAND WAVE</button>
          <button class="action-btn" onclick="sendCmd('V')">BODY DANCE</button>
        </div>
      </div>

      <div class="action-group">
        <h3>UTILITIES</h3>
        <div class="btn-grid" style="grid-template-columns: 1fr;">
          <button id="ledCycleBtn" class="action-btn toggle" onclick="cycleLedMode()">ILLUMINATION: OFF</button>
          <button class="action-btn" onclick="window.location.href='/update'">SYS UPDATE</button>
        </div>
      </div>
    </section>
  </main>

  <footer class="footer">
    <div>SYS: <span class="footer-highlight">ONLINE</span></div>
    <div>PING: <span class="footer-highlight">&lt; 5ms</span></div>
    <div>CTRL: <span class="footer-highlight">ACTIVE</span></div>
  </footer>

  <script>
    let activeMotionCmd = '';
    let ledCycleState = 0;

    function setStatus(cmd, message) {
      document.getElementById('cmd').textContent = cmd;
      document.getElementById('currentCmd').textContent = cmd || 'NONE';
      document.getElementById('msg').textContent = message;
      document.getElementById('currentMsg').textContent = message.toUpperCase();
    }

    function cycleLedMode() {
      const btn = document.getElementById('ledCycleBtn');
      if (ledCycleState === 0) {
        ledCycleState = 1;
        btn.textContent = 'ILLUMINATION: ON';
        btn.className = 'action-btn toggle on';
        sendCmd('O');
      } else if (ledCycleState === 1) {
        ledCycleState = 2;
        btn.textContent = 'ILLUMINATION: BLINK';
        btn.className = 'action-btn toggle blink';
        sendCmd('K');
      } else {
        ledCycleState = 0;
        btn.textContent = 'ILLUMINATION: OFF';
        btn.className = 'action-btn toggle';
        sendCmd('X');
      }
    }

    async function sendCmd(cmd, fireAndForget = false) {
      try {
        if (!fireAndForget) {
          setStatus(document.getElementById('cmd').textContent, 'TX: ' + cmd + '...');
        }
        const res = await fetch('/cmd?go=' + encodeURIComponent(cmd), { cache: 'no-store' });
        const text = await res.text();
        
        if (!fireAndForget) {
          setStatus(cmd, text);
        } else {
          setStatus(cmd, document.getElementById('currentMsg').textContent);
        }
      } catch (e) {
        if (!fireAndForget) {
          setStatus(document.getElementById('cmd').textContent, 'LINK_ERR');
        }
      }
    }

    function startHold(cmd) {
      if (activeMotionCmd === cmd) return;
      stopHold(true);
      activeMotionCmd = cmd;
      setStatus(cmd, 'HOLD: ' + cmd);
      sendCmd(cmd, true);
    }

    function stopHold(silent) {
      if (!activeMotionCmd) return;
      activeMotionCmd = '';
      sendCmd('S', true);
      if (!silent) setStatus('S', 'HALT');
    }

    for (const btn of document.querySelectorAll('button.hold')) {
      const cmd = btn.dataset.cmd;
      btn.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        btn.setPointerCapture(e.pointerId);
        startHold(cmd);
      });
      btn.addEventListener('pointerup', () => stopHold(false));
      btn.addEventListener('pointerleave', () => stopHold(false));
      btn.addEventListener('pointercancel', () => stopHold(false));
      btn.addEventListener('lostpointercapture', () => stopHold(false));
      btn.addEventListener('contextmenu', (e) => e.preventDefault());
    }

    document.addEventListener('contextmenu', (e) => e.preventDefault());
    window.addEventListener('blur', () => stopHold(true));
    window.addEventListener('pagehide', () => stopHold(true));
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState !== 'visible') stopHold(true);
    });
  </script>
</body>
</html>
)rawliteral";

void sendRobotCommand(const char command) {
  Serial2.write(command);
}

void handleRoot() {
  server.send_P(200, "text/html", indexHtml);
}

void handleCommand() {
  if (!server.hasArg("go") || server.arg("go").length() == 0) {
    server.send(400, "text/plain", "Missing command");
    return;
  }

  const char command = server.arg("go")[0];
  switch (command) {
    case 'F':
    case 'B':
    case 'L':
    case 'R':
    case 'S':
    case 'P':
    case 'Q':
    case 'U':
    case 'W':
    case 'V':
    case 'O':
    case 'X':
    case 'K':
      sendRobotCommand(command);
      server.send(200, "text/plain", String("Sent ") + command);
      break;
    default:
      server.send(400, "text/plain", "Invalid command");
      break;
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(ROBOT_BAUD, SERIAL_8N1, ESP32_RX_PIN, ESP32_TX_PIN);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  WiFi.softAP(apSsid, apPassword);

  server.on("/", handleRoot);
  server.on("/cmd", handleCommand);
  server.onNotFound(handleNotFound);
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send_P(200, "text/html", updateHtml);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "UPDATE FAILED!" : "UPDATE SUCCESS! Rebooting...");
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();

  Serial.println();
  Serial.println("ESP32 Web Controller started");
  Serial.print("Reset reason: ");
  Serial.println((int)esp_reset_reason());
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  delay(2);
}
