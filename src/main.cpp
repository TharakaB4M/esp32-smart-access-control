#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secrets.h"

// ---- Wi-Fi credentials loaded from the local .env file at build time ----
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* fallbackApName = WIFI_AP_NAME;
const char* fallbackApPassword = WIFI_AP_PASSWORD;

// ---- LED pins ----
const int GREEN_LED  = 25;
const int YELLOW_LED = 26;
const int RED_LED    = 27;

WebServer server(80);

// ---- Login state tracking ----
int failedAttempts = 0;
bool systemLocked = false;

// ---- Lockout LED blink tracking ----
unsigned long lastBlinkTime = 0;
bool redLedState = false;

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Smart Access Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      --ink: #eef4f5;
      --muted: #91a5aa;
      --panel: rgba(17, 31, 35, 0.92);
      --line: #294247;
      --cyan: #54d6d2;
      --cyan-dark: #123d40;
      --coral: #ff806d;
      --yellow: #f5c96a;
      --shadow: 0 24px 70px rgba(0, 0, 0, 0.34);
    }
    * { box-sizing: border-box; }
    body {
      min-height: 100vh;
      margin: 0;
      padding: 24px;
      color: var(--ink);
      background-color: #091619;
      background-image: linear-gradient(135deg, rgba(84, 214, 210, 0.08) 0, transparent 42%), linear-gradient(315deg, rgba(255, 128, 109, 0.07) 0, transparent 36%);
      font-family: Georgia, 'Times New Roman', serif;
    }
    .shell {
      display: grid;
      grid-template-columns: minmax(250px, 0.78fr) minmax(320px, 1.22fr);
      width: min(920px, 100%);
      min-height: 590px;
      margin: auto;
      overflow: hidden;
      border: 1px solid var(--line);
      border-radius: 18px;
      background: var(--panel);
      box-shadow: var(--shadow);
    }
    .intro {
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      padding: clamp(28px, 5vw, 58px);
      background: linear-gradient(155deg, #15383b, #10262a 58%, #1f292b);
    }
    .eyebrow {
      margin: 0 0 22px;
      color: var(--cyan);
      font-family: 'Courier New', monospace;
      font-size: 11px;
      font-weight: bold;
      letter-spacing: 2px;
      text-transform: uppercase;
    }
    h1 {
      max-width: 330px;
      margin: 0;
      font-size: clamp(34px, 5vw, 55px);
      line-height: 0.98;
      font-weight: normal;
    }
    .intro-copy {
      max-width: 270px;
      margin: 24px 0 0;
      color: var(--muted);
      font-size: 15px;
      line-height: 1.65;
    }
    .signal {
      display: flex;
      align-items: center;
      gap: 10px;
      color: var(--muted);
      font-family: 'Courier New', monospace;
      font-size: 12px;
    }
    .signal-dot {
      width: 9px;
      height: 9px;
      border-radius: 50%;
      background: var(--cyan);
      box-shadow: 0 0 0 5px rgba(84, 214, 210, 0.12);
    }
    .workspace {
      display: flex;
      align-items: center;
      padding: clamp(28px, 5vw, 58px);
      background: #0e1c20;
    }
    .card { width: 100%; }
    .section-label {
      margin: 0 0 11px;
      color: var(--muted);
      font-family: 'Courier New', monospace;
      font-size: 11px;
      letter-spacing: 1.4px;
      text-transform: uppercase;
    }
    h2 { margin: 0 0 30px; font-size: 27px; font-weight: normal; }
    .field { display: block; margin-bottom: 17px; }
    .field span {
      display: block;
      margin-bottom: 7px;
      color: #bfd0d2;
      font-size: 13px;
    }
    input {
      width: 100%;
      padding: 14px 15px;
      border: 1px solid var(--line);
      border-radius: 7px;
      outline: none;
      color: var(--ink);
      background: #0a171a;
      font: 15px 'Courier New', monospace;
      transition: border-color 0.2s, box-shadow 0.2s;
    }
    input:focus { border-color: var(--cyan); box-shadow: 0 0 0 3px rgba(84, 214, 210, 0.12); }
    input:disabled { cursor: not-allowed; opacity: 0.45; }
    button {
      width: 100%;
      margin-top: 8px;
      padding: 15px;
      border: 0;
      border-radius: 7px;
      color: #071719;
      background: var(--cyan);
      font: bold 14px 'Courier New', monospace;
      letter-spacing: 0.5px;
      cursor: pointer;
      transition: transform 0.2s, background 0.2s;
    }
    button:hover:not(:disabled) { transform: translateY(-2px); background: #86e7df; }
    button:disabled { cursor: not-allowed; opacity: 0.42; }
    #resetBtn {
      display: none;
      margin-top: 12px;
      color: var(--ink);
      background: #5d2927;
    }
    #resetBtn:hover:not(:disabled) { background: #7a3834; }
    .status-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 14px;
      margin-top: 28px;
      padding: 15px 0;
      border-top: 1px solid var(--line);
      border-bottom: 1px solid var(--line);
    }
    #status { margin: 0; color: var(--yellow); font-size: 15px; font-weight: bold; }
    .status-icon {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: 17px;
      height: 17px;
      margin-right: 6px;
      border: 1px solid currentColor;
      border-radius: 50%;
      font: bold 12px 'Courier New', monospace;
      vertical-align: -2px;
    }
    #attempts { margin: 0; color: var(--muted); font: 12px 'Courier New', monospace; }
    #alertBox {
      display: none;
      position: fixed;
      z-index: 10;
      top: 22px;
      left: 50%;
      width: min(340px, calc(100% - 40px));
      padding: 13px 16px;
      transform: translateX(-50%);
      border: 1px solid rgba(255, 128, 109, 0.55);
      border-radius: 7px;
      color: #ffe9e5;
      background: #5d2927;
      box-shadow: var(--shadow);
      font: 13px 'Courier New', monospace;
      text-align: center;
    }
    #homePage { display: none; width: 100%; text-align: center; }
    #homePage .access-mark { margin: 0 0 18px; color: var(--cyan); font-size: 44px; }
    #homePage h2 { margin-bottom: 12px; }
    #homePage p { margin: 0 0 28px; color: var(--muted); line-height: 1.6; }
    #logoutBtn { color: var(--ink); background: #284147; }
    #logoutBtn:hover:not(:disabled) { background: #395861; }
    .shake { animation: shake 0.4s; }
    @keyframes shake { 0%, 100% { transform: translateX(0); } 25% { transform: translateX(-7px); } 75% { transform: translateX(7px); } }
    @media (max-width: 680px) {
      body { padding: 0; }
      .shell { display: block; min-height: 100vh; border: 0; border-radius: 0; }
      .intro { min-height: 290px; padding: 34px 26px; }
      .intro-copy { margin-top: 16px; }
      .workspace { min-height: calc(100vh - 290px); padding: 34px 26px; }
    }
  </style>
</head>
<body>
  <div id="alertBox"></div>

  <main class="shell">
    <section class="intro">
      <div>
        <p class="eyebrow">Secure facility / node 01</p>
        <h1>Smart access control.</h1>
        <p class="intro-copy">Authenticate to open the secure area. This terminal is monitored and will lock after three unsuccessful attempts.</p>
      </div>
      <div class="signal"><span class="signal-dot"></span> ESP32 NETWORK ONLINE</div>
    </section>

    <section class="workspace">
      <div class="card" id="loginCard">
        <p class="section-label">Identity verification</p>
        <h2>Sign in to continue</h2>
        <label class="field" for="username"><span>Username</span><input type="text" id="username" placeholder="Enter username" autocomplete="username"></label>
        <label class="field" for="password"><span>Password</span><input type="password" id="password" placeholder="Enter password" autocomplete="current-password"></label>
        <button id="loginBtn" type="button" onclick="login()">VERIFY CREDENTIALS</button>
        <button id="resetBtn" type="button" onclick="restartSystem()">RESTART SYSTEM</button>
        <div class="status-row"><p id="status"><span class="status-icon">!</span> System Ready</p><p id="attempts">Failed Attempts: 0 / 3</p></div>
      </div>

      <div id="homePage">
        <p class="access-mark">UNLOCKED</p>
        <p class="section-label">Identity verified</p>
        <h2>Access granted</h2>
        <p>Secure area unlocked for this session.</p>
        <button id="logoutBtn" type="button" onclick="logout()">END SESSION</button>
      </div>
    </section>
  </main>

<script>
function showAlert(message) {
  const box = document.getElementById('alertBox');
  box.innerText = message;
  box.style.display = 'block';
  setTimeout(() => { box.style.display = 'none'; }, 2500);
}

function login() {
  const username = document.getElementById('username').value;
  const password = document.getElementById('password').value;

  if (username.trim() === '' || password.trim() === '') {
    document.getElementById('status').innerHTML = '<span class="status-icon">!</span> Fields required';
    showAlert('Enter both username and password');
    return;
  }

  fetch(`/login?username=${encodeURIComponent(username)}&password=${encodeURIComponent(password)}`)
    .then(response => response.json())
    .then(data => {
      document.getElementById('status').innerText = getStatusText(data);
      document.getElementById('attempts').innerText = `Failed Attempts: ${data.attempts} / 3`;

      if (data.success) {
        document.getElementById('loginCard').style.display = 'none';
        document.getElementById('homePage').style.display = 'block';
        return;
      }

      const card = document.getElementById('loginCard');
      card.classList.remove('shake');
      void card.offsetWidth;
      card.classList.add('shake');

      if (data.locked) {
        showAlert('SYSTEM LOCKED - Too many failed attempts');
        document.getElementById('username').disabled = true;
        document.getElementById('password').disabled = true;
        document.getElementById('loginBtn').disabled = true;
        document.getElementById('resetBtn').style.display = 'block';
      } else {
        showAlert('ACCESS DENIED - Incorrect username or password');
      }
    })
    .catch(err => {
      document.getElementById('status').innerText = 'CONNECTION ERROR';
      console.error(err);
    });
}

function restartSystem() {
  const resetButton = document.getElementById('resetBtn');
  resetButton.disabled = true;
  resetButton.innerText = 'RESTARTING...';

  fetch('/reset')
    .then(() => {
      setTimeout(() => window.location.reload(), 1500);
    })
    .catch(() => {
      resetButton.disabled = false;
      resetButton.innerText = 'RESTART SYSTEM';
      showAlert('Restart failed - Press the ESP32 reset button');
    });
}

function logout() {
  fetch('/logout')
    .then(() => {
      document.getElementById('homePage').style.display = 'none';
      document.getElementById('loginCard').style.display = 'block';
      document.getElementById('username').value = '';
      document.getElementById('password').value = '';
      document.getElementById('status').innerHTML = '<span class="status-icon">!</span> System Ready';
      document.getElementById('attempts').innerText = 'Failed Attempts: 0 / 3';
    })
    .catch(() => showAlert('Unable to end session'));
}

function getStatusText(data) {
  if (data.locked) return 'SYSTEM LOCKED';
  if (data.success) return 'ACCESS GRANTED';
  return 'ACCESS DENIED';
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleReset() {
  server.send(200, "text/plain", "Restarting system");
  delay(200);
  ESP.restart();
}

void handleLogout() {
  if (!systemLocked) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    redLedState = false;
  }

  server.send(200, "text/plain", "Session ended");
}

void handleLogin() {
  if (systemLocked) {
    server.send(200, "application/json",
      "{\"success\":false,\"status\":\"LOCKED\",\"message\":\"System is locked\",\"attempts\":" +
      String(failedAttempts) + ",\"locked\":true}");
    return;
  }

  String username = server.arg("username");
  String password = server.arg("password");

  String jsonResponse;

  if (username == "admin" && password == "1234") {
    failedAttempts = 0;
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);

    jsonResponse = "{\"success\":true,\"status\":\"ACCESS_GRANTED\",\"message\":\"Access Granted\",\"attempts\":0,\"locked\":false}";
  } else {
    failedAttempts++;
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);

    if (failedAttempts >= 3) {
      systemLocked = true;
      jsonResponse = "{\"success\":false,\"status\":\"SYSTEM_LOCKED\",\"message\":\"System Locked\",\"attempts\":" +
                      String(failedAttempts) + ",\"locked\":true}";
    } else {
      jsonResponse = "{\"success\":false,\"status\":\"ACCESS_DENIED\",\"message\":\"Access Denied\",\"attempts\":" +
                      String(failedAttempts) + ",\"locked\":false}";
    }
  }

  server.send(200, "application/json", jsonResponse);
}

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, HIGH);
  digitalWrite(RED_LED, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  const unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartTime < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed. Starting fallback access point.");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(fallbackApName, fallbackApPassword);
    Serial.print("Fallback access point IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  if (MDNS.begin("jiat")) {
    Serial.println("mDNS responder started: http://jiat.local");
  } else {
    Serial.println("Error setting up mDNS responder!");
  }

  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.on("/reset", handleReset);
  server.on("/logout", handleLogout);
  server.begin();
  Serial.println("HTTP server started on port 80");
}

void loop() {
  server.handleClient();

  if (systemLocked) {
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= 300) {
      redLedState = !redLedState;
      digitalWrite(RED_LED, redLedState);
      lastBlinkTime = currentTime;
    }
  }
}
