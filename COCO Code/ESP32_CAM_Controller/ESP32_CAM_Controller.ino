#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>

// -----------------------------------------------------------------------------
// ESP32-CAM Web Controller for the COCO spider robot
//
// Use this sketch on an ESP32-CAM board even if the camera module is damaged.
// It does not use any camera functions. It only provides a Wi-Fi access point
// and a control web page, then forwards commands to the robot controller over
// UART.
//
// Suggested ESP32-CAM wiring for AI Thinker boards:
//   U0T / GPIO1 -> Robot RX
//   U0R / GPIO3 <- Robot TX (optional, only if you want replies)
//   GND    -> Robot GND
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

static const uint32_t ROBOT_BAUD = 9600;

IPAddress apIP(192, 168, 4, 1);
IPAddress apGateway(192, 168, 4, 1);
IPAddress apSubnet(255, 255, 255, 0);

WebServer server(80);

const char indexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>COCO CAM Controller</title>
  <style>
    :root {
      --bg1: #030712;
      --bg2: #0b1733;
      --panel: rgba(7, 12, 22, 0.92);
      --panel-strong: rgba(12, 20, 39, 0.96);
      --accent: #3b82f6;
      --accent2: #ef4444;
      --accent3: #60a5fa;
      --text: #eef4ff;
      --muted: #a8b9dd;
      --border: rgba(96, 165, 250, 0.24);
    }
    * { box-sizing: border-box; }
    html, body, button {
      -webkit-touch-callout: none;
      -webkit-user-select: none;
      -ms-user-select: none;
      user-select: none;
    }
    html, body { min-height: 100%; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, Geneva, Verdana, sans-serif;
      color: var(--text);
      background:
        radial-gradient(circle at top, rgba(59, 130, 246, 0.20), transparent 30%),
        radial-gradient(circle at 15% 80%, rgba(239, 68, 68, 0.16), transparent 25%),
        radial-gradient(circle at 85% 15%, rgba(59, 130, 246, 0.14), transparent 22%),
        linear-gradient(160deg, var(--bg1), var(--bg2));
      display: grid;
      place-items: center;
      padding: 12px;
      touch-action: manipulation;
    }
    .card {
      width: min(1040px, 100%);
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 20px;
      box-shadow: 0 34px 110px rgba(0, 0, 0, 0.52);
      backdrop-filter: blur(14px);
      overflow: hidden;
      position: relative;
    }
    .card::before {
      content: '';
      position: absolute;
      inset: 0;
      background:
        radial-gradient(circle at 20% 20%, rgba(59, 130, 246, 0.10), transparent 22%),
        radial-gradient(circle at 80% 25%, rgba(239, 68, 68, 0.10), transparent 20%),
        linear-gradient(135deg, rgba(255,255,255,0.08), transparent 30%, transparent 70%, rgba(255,255,255,0.04));
      pointer-events: none;
    }
    .hero {
      position: relative;
      z-index: 1;
      display: flex;
      justify-content: space-between;
      gap: 16px;
      align-items: center;
      margin-bottom: 18px;
    }
    h1 {
      margin: 0 0 8px;
      font-size: clamp(1.8rem, 4vw, 2.45rem);
      letter-spacing: 0.01em;
      line-height: 1.05;
    }
    .subtitle {
      margin: 0;
      color: var(--muted);
      line-height: 1.5;
      max-width: 32rem;
    }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 10px 14px;
      border-radius: 8px;
      background: rgba(255,255,255,0.06);
      border: 1px solid var(--border);
      color: var(--text);
      font-size: 0.9rem;
      white-space: nowrap;
      flex: 0 0 auto;
    }
    .layout {
      position: relative;
      z-index: 1;
      display: grid;
      grid-template-columns: 1.15fr 0.9fr 1.15fr;
      gap: 16px;
      align-items: stretch;
    }
    .panel {
      background: rgba(255,255,255,0.04);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 16px;
      min-height: 390px;
    }
    .panel h2 {
      margin: 0 0 12px;
      font-size: 1rem;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      color: var(--muted);
    }
    .status {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      align-items: center;
      margin-bottom: 18px;
      padding: 14px 16px;
      border-radius: 8px;
      background: var(--panel-strong);
      border: 1px solid var(--border);
      font-size: 0.95rem;
      position: relative;
      z-index: 1;
    }
    .status.center-brand {
      justify-content: center;
      text-align: center;
      min-height: 78px;
      margin-bottom: 18px;
      gap: 0;
    }
    .status-label { color: var(--muted); }
    .brand-copy {
      display: grid;
      gap: 4px;
      justify-items: center;
    }
    .brand-kicker {
      color: var(--accent2);
      font-size: 0.78rem;
      letter-spacing: 0.22em;
      text-transform: uppercase;
      font-weight: 800;
    }
    .brand-title {
      color: var(--text);
      font-size: clamp(1.15rem, 2.8vw, 1.7rem);
      letter-spacing: 0.08em;
      text-transform: uppercase;
      font-weight: 900;
    }
    .sr-only {
      position: absolute;
      width: 1px;
      height: 1px;
      padding: 0;
      margin: -1px;
      overflow: hidden;
      clip: rect(0, 0, 0, 0);
      white-space: nowrap;
      border: 0;
    }
    .dpad {
      position: relative;
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 12px;
      align-content: center;
      height: calc(100% - 34px);
    }
    .dpad .spacer {
      min-height: 64px;
      border-radius: 8px;
      background: transparent;
      box-shadow: none;
    }
    .dpad .center {
      min-height: 92px;
      border-radius: 6px;
      background: rgba(255,255,255,0.06);
      border: 1px solid var(--border);
      box-shadow: inset 0 0 0 1px rgba(255,255,255,0.03);
    }
    button {
      border: 0;
      border-radius: 8px;
      padding: 16px 12px;
      font-size: 1rem;
      font-weight: 800;
      letter-spacing: 0.01em;
      color: #07101d;
      background: linear-gradient(180deg, #e2e8f0, #cbd5e1);
      cursor: pointer;
      transition: transform 0.14s ease, filter 0.14s ease, box-shadow 0.14s ease;
      min-height: 64px;
      box-shadow: 0 10px 24px rgba(0, 0, 0, 0.18);
      -webkit-tap-highlight-color: transparent;
      touch-action: none;
    }
    button:hover { transform: translateY(-1px); filter: brightness(1.02); }
    button:active { transform: scale(0.97); filter: brightness(0.95); }
    .forward {
      background: linear-gradient(180deg, #93c5fd, #2563eb);
      font-size: 1.08rem;
      min-height: 110px;
      grid-column: 2;
    }
    .back { background: linear-gradient(180deg, #fca5a5, #dc2626); color: #fff; }
    .left, .right { background: linear-gradient(180deg, #bfdbfe, #3b82f6); }
    .gesture { background: linear-gradient(180deg, #dbeafe, #2563eb); color: #fff; }
    .basic-pos { background: linear-gradient(180deg, #bfdbfe, #1d4ed8); color: #fff; }
    .spider-pos { background: linear-gradient(180deg, #fecaca, #ef4444); color: #fff; }
    .led-on { background: linear-gradient(180deg, #93c5fd, #2563eb); color: #fff; }
    .led-off { background: linear-gradient(180deg, #fecaca, #b91c1c); color: #fff; }
    .led-blink { background: linear-gradient(180deg, #60a5fa, #ef4444); color: #fff; }
    .wide { grid-column: span 3; }
    .nav-left, .nav-right { min-height: 80px; }
    .nav-center {
      min-height: 110px;
      display: grid;
      place-items: center;
      box-shadow: 0 18px 36px rgba(59, 130, 246, 0.28);
    }
    .dpad-btn {
      position: relative;
      display: grid;
      place-items: center;
      font-size: 1.35rem;
      font-weight: 900;
      color: #061018;
      border-radius: 6px;
      clip-path: polygon(18% 0, 82% 0, 100% 18%, 100% 82%, 82% 100%, 18% 100%, 0 82%, 0 18%);
      box-shadow: inset 0 -8px 18px rgba(0, 0, 0, 0.18), 0 10px 24px rgba(0, 0, 0, 0.18);
    }
    .dpad-up {
      grid-column: 2;
      background: linear-gradient(180deg, #93c5fd, #2563eb);
      min-height: 92px;
    }
    .dpad-left {
      background: linear-gradient(180deg, #bfdbfe, #3b82f6);
      min-height: 78px;
    }
    .dpad-right {
      background: linear-gradient(180deg, #fca5a5, #ef4444);
      min-height: 78px;
    }
    .dpad-down {
      grid-column: 2;
      background: linear-gradient(180deg, #fecaca, #dc2626);
      min-height: 92px;
    }
    .pill {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 6px 10px;
      border-radius: 6px;
      background: rgba(255,255,255,0.08);
      color: var(--muted);
      font-size: 0.8rem;
      margin-bottom: 8px;
    }
    .control-pad {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 12px;
      align-items: center;
      justify-items: stretch;
    }
    .action-stack {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      align-content: start;
    }
    .action-stack .action-title {
      grid-column: 1 / -1;
      margin: 0 0 2px;
      color: var(--muted);
      font-size: 0.82rem;
      letter-spacing: 0.18em;
      text-transform: uppercase;
      font-weight: 800;
    }
    .action-stack button {
      grid-column: auto;
      min-height: 72px;
      border-radius: 8px;
      box-shadow: inset 0 0 0 1px rgba(255,255,255,0.06), 0 10px 24px rgba(0, 0, 0, 0.18);
    }
    .action-stack .stance {
      background: linear-gradient(180deg, #bfdbfe, #1d4ed8);
      color: #fff;
    }
    .action-stack .gesture {
      background: linear-gradient(180deg, #dbeafe, #2563eb);
      color: #fff;
    }
    .action-stack .power {
      background: linear-gradient(180deg, #fecaca, #dc2626);
      color: #fff;
    }
    .top-row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-bottom: 12px;
    }
    .hint {
      margin-top: 18px;
      font-size: 0.9rem;
      color: var(--muted);
      position: relative;
      z-index: 1;
      display: flex;
      justify-content: space-between;
      gap: 12px;
      flex-wrap: wrap;
    }
    .pulse {
      color: var(--accent);
      font-weight: 700;
    }
    .footer-note {
      color: var(--muted);
    }
    @media (max-width: 560px) {
      .card { padding: 16px; border-radius: 22px; }
      .hero { flex-direction: column; }
      .badge { align-self: flex-start; }
      .layout { grid-template-columns: 1fr; }
      .panel { min-height: auto; }
      .dpad, .action-stack { gap: 10px; }
      button { min-height: 60px; padding: 14px 10px; }
      .forward { min-height: 90px; }
      .dpad-up, .dpad-down { min-height: 84px; }
      .dpad-left, .dpad-right { min-height: 72px; }
      .hint { flex-direction: column; }
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="hero">
      <div>
        <div class="pill">ESP32-CAM Controller</div>
        <h1>Spider Robot Control</h1>
        <p class="subtitle">This board does not use the camera. Hold a direction to move, release to stop, and use the pose buttons for stance control.</p>
      </div>
      <div class="badge">Wi-Fi AP: <span class="pulse">SpiderRobot</span></div>
    </div>
    <div class="status center-brand">
      <div class="brand-copy">
        <div class="brand-kicker">Robotics</div>
        <div class="brand-title">COCO Controller</div>
      </div>
      <div class="sr-only">
        <span class="status-label">Last command</span> <span class="pulse" id="cmd">none</span>
        <span id="msg">Ready</span>
      </div>
    </div>
    <div class="layout">
      <div class="panel">
        <h2>Movement</h2>
        <div class="dpad">
          <div class="spacer"></div>
          <button class="forward hold nav-center dpad-btn dpad-up" data-cmd="F">▲</button>
          <div class="spacer"></div>
          <button class="left hold nav-left dpad-btn dpad-left" data-cmd="L">◀</button>
          <div class="center"></div>
          <button class="right hold nav-right dpad-btn dpad-right" data-cmd="R">▶</button>
          <div class="spacer"></div>
          <button class="back hold dpad-btn dpad-down" data-cmd="B">▼</button>
          <div class="spacer"></div>
        </div>
      </div>
      <div class="panel">
        <h2>Status</h2>
        <div class="top-row">
          <div class="badge" style="justify-content:center; width:100%;">Current: <span class="pulse" id="currentCmd">none</span></div>
          <div class="badge" style="justify-content:center; width:100%;">Mode: <span class="pulse">CAM</span></div>
        </div>
        <div class="status" style="margin-bottom:12px;">
          <div><span class="status-label">Message</span></div>
          <div id="currentMsg">Ready</div>
        </div>
        <div class="hint" style="margin-top:0;">
          <div>Hold a direction to move.</div>
          <div class="footer-note">Release to stop instantly.</div>
        </div>
      </div>
      <div class="panel">
        <h2>Actions</h2>
        <div class="action-stack">
          <div class="action-title">Poses</div>
          <button class="stance" onclick="sendCmd('P')">Basic Position</button>
          <button class="power" onclick="sendCmd('Q')">Spider Position</button>
          <div class="action-title">Gestures</div>
          <button class="gesture" onclick="sendCmd('U')">Hand Shake</button>
          <button class="gesture" onclick="sendCmd('W')">Hand Wave</button>
          <button class="gesture" onclick="sendCmd('V')">Body Dance</button>
          <button class="led-off" id="ledCycleBtn" onclick="cycleLedMode()">LED Off</button>
        </div>
      </div>
    </div>
    <div class="hint">
      <div>Wi-Fi password: <span class="pulse">12345678</span></div>
      <div class="footer-note">Landscape gamepad layout for phone or tablet use.</div>
    </div>
  </div>
  <script>
    let activeMotionCmd = '';
    let ledCycleState = 0;

    function setStatus(cmd, message) {
      document.getElementById('cmd').textContent = cmd;
      document.getElementById('currentCmd').textContent = cmd;
      document.getElementById('msg').textContent = message;
      document.getElementById('currentMsg').textContent = message;
    }

    function setLedButton(label, styleClass) {
      const button = document.getElementById('ledCycleBtn');
      button.textContent = label;
      button.className = styleClass;
    }

    async function sendCmd(cmd, fireAndForget = false) {
      try {
        if (!fireAndForget) {
          setStatus(document.getElementById('cmd').textContent, 'Sending ' + cmd + '...');
        }

        const res = await fetch('/cmd?go=' + encodeURIComponent(cmd), {
          cache: 'no-store'
        });

        const text = await res.text();
        setStatus(cmd, fireAndForget ? document.getElementById('currentMsg').textContent : text);
        if (!fireAndForget) {
          setStatus(cmd, text);
        }
      } catch (e) {
        if (!fireAndForget) {
          setStatus(document.getElementById('cmd').textContent, 'Connection error');
        }
      }
    }

    function cycleLedMode() {
      if (ledCycleState === 0) {
        ledCycleState = 1;
        setLedButton('LED On', 'led-on');
        sendCmd('O');
        return;
      }

      if (ledCycleState === 1) {
        ledCycleState = 2;
        setLedButton('LED Blink', 'led-blink');
        sendCmd('K');
        return;
      }

      ledCycleState = 0;
      setLedButton('LED Off', 'led-off');
      sendCmd('X');
    }

    function startHold(cmd) {
      if (activeMotionCmd === cmd) {
        return;
      }

      stopHold(true);
      activeMotionCmd = cmd;
      setStatus(cmd, 'Holding ' + cmd);
      sendCmd(cmd, true);
    }

    function stopHold(silent) {
      if (!activeMotionCmd) {
        return;
      }

      activeMotionCmd = '';
      sendCmd('S', true);

      if (!silent) {
        setStatus('S', 'Stopped');
      }
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
      if (document.visibilityState !== 'visible') {
        stopHold(true);
      }
    });
  </script>
</body>
</html>
)rawliteral";

void sendRobotCommand(const char command) {
  Serial.write(command);
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
  Serial.begin(ROBOT_BAUD);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  WiFi.softAP(apSsid, apPassword);

  server.on("/", handleRoot);
  server.on("/cmd", handleCommand);
  server.onNotFound(handleNotFound);
  server.begin();

  // Do not print debug logs here: Serial is shared with the robot link on U0T/U0R.
}

void loop() {
  server.handleClient();
  delay(2);
}
