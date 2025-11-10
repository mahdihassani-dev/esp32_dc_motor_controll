#include <WiFi.h>
#include <WebServer.h>
#include "DYPlayerArduino.h" // کتابخانه ماژول موسیقی از کد ۱

// --- 1. تعاریف پین‌ها (ادغام شده از هر دو کد) ---

// موتور (از کد ۱ - با PWM)
const int ENA = 14; // پین کنترل سرعت (PWM)
const int IN1 = 27; // پین جهت ۱
const int IN2 = 26; // پین جهت ۲

// بخار (از هر دو کد)
const int STEAM_PIN = 15;

// ماژول موسیقی (از کد ۱)
// Serial2 به طور پیش‌فرض روی پین‌های 16 (RX) و 17 (TX) است

// سنسورها (از کد ۲)
const int PROBE_LOW_PIN = 19;     // سنسور سطح آب (پایین/متوسط)
const int PROBE_HIGH_PIN = 18;    // سنسور سطح آب (بالا)
const int REED_SWITCH_PIN = 32;   // سنسور ایستگاه (Reed Switch)

// --- 2. تنظیمات شبکه و سرور ---
const char* ssid = "Esp32";
const char* password = "1234567876543212";
WebServer server(80);

// --- 3. ماژول موسیقی ---
DY::Player player(&Serial2);

// --- 4. متغیرهای برنامه خودکار (از کد ۱) ---
bool autoRunning = false;
unsigned long autoStartTime = 0;
unsigned long totalDuration = 120000; // پیش‌فرض ۲ دقیقه

struct StopEvent {
  unsigned long startTime;
  unsigned long duration;
};
struct SteamEvent {
  unsigned long startTime;
  unsigned long duration;
};
struct MusicEvent {
  unsigned long startTime;
  int songNumber;
  int repeats;
  int currentRepeat = 0;
  bool playing = false;
};

StopEvent stops[10];
int numStops = 0;
SteamEvent steams[10];
int numSteams = 0;
MusicEvent musics[10];
int numMusics = 0;

// --- 5. متغیرهای وضعیت سراسری (ادغام شده) ---
bool motorRunning = false;
int currentSpeed = 200; // سرعت پیش‌فرض (از کد ۱)
bool currentDirection = true; // true = جلو
bool steamActive = false;

// وضعیت آب (از کد ۲)
String tankStatus = "در حال بررسی...";
unsigned long lastWaterCheck = 0;
const unsigned long WATER_CHECK_INTERVAL = 500; // 500ms

// منطق توقف اضطراری (از کد ۲)
bool waitingForRefillStop = false;
bool motorLockedDueToEmptyTank = false;

// --- 6. توابع کمکی سنسورها (از کد ۲) ---

// خواندن پایدار پین (فیلتر نویز)
bool stableRead(int pin) {
  int zeros = 0;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(pin) == LOW) zeros++;
    delay(2);
  }
  return (zeros >= 6); // LOW = آب یا مگنت شناسایی شد
}

// تعیین وضعیت تانک
String getTankStatus(bool low, bool high) {
  if (high) return "پر";
  if (low)  return "نیمه‌پر";
  return "خالی";
}

// --- 7. توابع کمکی کنترل (ادغام شده) ---

void stopMotor() {
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  motorRunning = false;
}

// 🛑 تابع startMotor به‌روزرسانی شده تا قفل را بررسی کند
void startMotor() {
  // اگر موتور قفل است، اجازه حرکت نده
  if (motorLockedDueToEmptyTank) {
    stopMotor();
    return;
  }
  digitalWrite(IN1, currentDirection ? HIGH : LOW);
  digitalWrite(IN2, currentDirection ? LOW : HIGH);
  analogWrite(ENA, currentSpeed);
  motorRunning = true;
}

void stopAll() {
  stopMotor();
  digitalWrite(STEAM_PIN, LOW);
  steamActive = false;
  player.stop();
  for (int i = 0; i < numMusics; i++) {
    musics[i].playing = false;
    musics[i].currentRepeat = 0;
  }
}

// تنظیم برنامه پیش‌فرض (از کد ۱)
void setDefaultProgram() {
  totalDuration = 120000;
  numStops = 2;
  stops[0] = {30000, 15000};
  stops[1] = {75000, 15000};
  numSteams = 3;
  steams[0] = {0, 30000};
  steams[1] = {45000, 30000};
  steams[2] = {90000, 30000};
  numMusics = 3;
  musics[0] = {0, 1, 1};
  musics[1] = {35000, 2, 1};
  musics[2] = {80000, 3, 1};
}

// --- 8. Setup (ادغام شده) ---
void setup() {
  Serial.begin(115200);

  // راه‌اندازی پین‌های خروجی
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(STEAM_PIN, OUTPUT);

  // راه‌اندازی پین‌های سنسور (ورودی با PULLUP)
  pinMode(PROBE_LOW_PIN, INPUT_PULLUP);
  pinMode(PROBE_HIGH_PIN, INPUT_PULLUP);
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP);

  // توقف همه‌چیز در ابتدا
  stopAll();

  // راه‌اندازی ماژول موسیقی
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  player.begin();
  player.setVolume(25);
  player.setCycleMode(DY::PlayMode::OneOff);

  // بارگذاری برنامه پیش‌فرض
  setDefaultProgram();

  // اتصال به WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // راه‌اندازی سرور (ادغام مسیرها)
  server.on("/", HTTP_GET, handleRoot);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/pin15", HTTP_GET, handlePin15);  // (برای بخار)
  server.on("/play", HTTP_GET, handlePlay);
  server.on("/pause", HTTP_GET, handlePause);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/volume", HTTP_GET, handleVolume);
  server.on("/auto_setup", HTTP_GET, handleAutoSetup);
  server.on("/start_auto", HTTP_GET, handleStartAuto);
  server.on("/stop_auto", HTTP_GET, handleStopAuto);
  server.on("/reset_default", HTTP_GET, handleResetDefault);
  server.on("/status", HTTP_GET, handleStatus); // 🆕 مسیر جدید برای وضعیت زنده

  server.begin();
  Serial.println("Web server started.");
}

// --- 9. Loop (منطق ادغام شده) ---
void loop() {
  server.handleClient();

  // --- بخش ۱: منطق سنسورها (از کد ۲) ---
  // این بخش همیشه اجرا می‌شود تا وضعیت قفل‌ها را مدیریت کند

  // ۱.الف: بررسی سطح آب (هر ۵۰۰ میلی‌ثانیه)
  if (millis() - lastWaterCheck >= WATER_CHECK_INTERVAL) {
    lastWaterCheck = millis();
    // بررسی سطح آب فقط زمانی که بخار خاموش است
    if (!steamActive) {
      bool low = stableRead(PROBE_LOW_PIN);
      bool high = stableRead(PROBE_HIGH_PIN);
      tankStatus = getTankStatus(low, high);

      // اگر مخزن خالی شد و موتور روشن بود، منتظر ایستگاه شو
      if (tankStatus == "خالی" && motorRunning && !waitingForRefillStop && !motorLockedDueToEmptyTank) {
        waitingForRefillStop = true;
        Serial.println("Water EMPTY. Waiting for refill station...");
      }

      // اگر موتور قفل بود و مخزن پر شد، قفل را باز کن
      if (motorLockedDueToEmptyTank && tankStatus == "پر") {
        Serial.println("Tank FULL. Motor unlocked.");
        motorLockedDueToEmptyTank = false;
      }
    }
  }

  // ۱.ب: بررسی ایستگاه (Reed Switch)
  if (waitingForRefillStop) {
    // اگر ایستگاه شناسایی شد
    if (stableRead(REED_SWITCH_PIN)) {
      Serial.println("Refill station detected. Stopping and LOCKING motor.");
      stopMotor(); // 🛑 استفاده از stopMotor کد ۱ (که ENA را کنترل می‌کند)
      waitingForRefillStop = false;
      motorLockedDueToEmptyTank = true; // موتور را قفل کن
    }
  }

  // --- بخش ۲: منطق برنامه خودکار (از کد ۱) ---
  if (autoRunning) {
    unsigned long currentTime = millis() - autoStartTime;

    // پایان برنامه
    if (currentTime >= totalDuration) {
      stopAll();
      autoRunning = false;
      return;
    }

    // 🛑 بررسی قفل اضطراری
    if (motorLockedDueToEmptyTank) {
      stopMotor(); // اطمینان از توقف موتور
    }

    // مدیریت توقف‌ها
    bool inStop = false;
    for (int i = 0; i < numStops; i++) {
      if (currentTime >= stops[i].startTime && currentTime < stops[i].startTime + stops[i].duration) {
        stopMotor();
        inStop = true;
        break;
      }
    }
    // 🛑 شروع مجدد موتور فقط اگر در توقف برنامه‌ریزی شده نباشد و قفل هم نباشد
    if (!inStop && !motorRunning && !motorLockedDueToEmptyTank) {
      startMotor(); // startMotor جدید ما، قفل را بررسی می‌کند
    }

    // مدیریت بخار (بدون تغییر)
    bool shouldSteam = false;
    for (int i = 0; i < numSteams; i++) {
      if (currentTime >= steams[i].startTime && currentTime < steams[i].startTime + steams[i].duration) {
        shouldSteam = true;
        break;
      }
    }
    if (shouldSteam && !steamActive) {
      digitalWrite(STEAM_PIN, HIGH);
      steamActive = true;
    } else if (!shouldSteam && steamActive) {
      digitalWrite(STEAM_PIN, LOW);
      steamActive = false;
    }

    // مدیریت موسیقی (بدون تغییر)
    for (int i = 0; i < numMusics; i++) {
      if (currentTime >= musics[i].startTime && !musics[i].playing && musics[i].currentRepeat < musics[i].repeats) {
        player.playSpecified(musics[i].songNumber);
        musics[i].playing = true;
      }
      if (musics[i].playing && player.getPlayingSound() == 0) {
        musics[i].currentRepeat++;
        musics[i].playing = false;
        if (musics[i].currentRepeat < musics[i].repeats) {
          player.playSpecified(musics[i].songNumber);
          musics[i].playing = true;
        }
      }
    }
  }
}

// --- 10. توابع Handlers (ادغام شده) ---

// 🆕 سرویس‌دهنده اصلی UI
void handleRoot() {
  // از کد HTML کد ۱ استفاده می‌کنیم، اما با تغییرات اساسی در جاوا اسکریپت
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>🚂 کنترلر یکپارچه ESP32</title>
<style>
  :root {
    --primary: #4a90e2; --secondary: #f5f5f5; --bg: #1c1c28;
    --card-bg: #2a2a3c; --text-color: #ffffff; --accent: #50e3c2;
    --warning: #f5a623; --danger: #d9534f;
  }
  * { box-sizing: border-box; font-family: "Segoe UI", "Roboto", sans-serif; }
  body { margin:0; padding:0; background: var(--bg); color: var(--text-color); display:flex; flex-direction:column; height:100vh; }
  h1,h3 { margin:0 0 15px 0; color: var(--text-color); text-align:center; }
  .content { flex:1; overflow-y:auto; padding:15px; display:flex; justify-content:center; align-items:flex-start;}
  .card { background: var(--card-bg); border-radius:16px; padding:20px; margin:10px; width:100%; max-width:400px; box-shadow: 0 4px 12px rgba(0,0,0,0.4); display:none; flex-direction:column; }
  .card.active { display:flex; }
  .btn { background: var(--primary); color: var(--text-color); border: none; border-radius: 8px; padding:12px 15px; margin:5px; cursor:pointer; font-size:1rem; transition: background-color 0.2s; flex-grow: 1; }
  .btn:hover:not(:disabled) { background: #3a80d2; }
  .btn:disabled { background: #555; cursor: not-allowed; opacity: 0.6; }
  .btn-group { display:flex; justify-content:center; flex-wrap: wrap; margin-top: 10px; }
  
  /* 🆕 نوار وضعیت جدید */
  .status-bar { margin-top:15px; font-weight:bold; font-size:1rem; color: var(--accent); text-align:center; min-height: 20px; padding: 10px; border-radius: 8px; }
  .status-info { background-color: rgba(74, 144, 226, 0.2); color: var(--accent); }
  .status-warning { background-color: rgba(245, 166, 35, 0.2); color: var(--warning); }
  .status-danger { background-color: rgba(217, 83, 79, 0.2); color: var(--danger); }
  
  /* 🆕 منطق قفل از کد ۲ */
  fieldset { border: none; padding: 0; margin: 0; }
  fieldset:disabled { opacity: 0.5; }

  label { display:block; margin:15px 0 5px 0; font-weight:bold; text-align:center; }
  input[type=range], input[type=number], input[type=text] { width:100%; padding:8px; border-radius:8px; border:1px solid var(--primary); background:var(--secondary); color:var(--bg); }
  .bottom-nav { display:flex; justify-content:space-around; background: var(--card-bg); padding:10px 0; border-top:1px solid rgba(255,255,255,0.1); }
  .nav-btn { flex:1; text-align:center; color: var(--text-color); font-weight:bold; cursor:pointer; padding:10px 0; transition: color 0.2s, border-top 0.2s; border-top: 3px solid transparent; }
  .nav-btn.active { color: var(--accent); border-top: 3px solid var(--accent); }
  .event-list { margin-top:10px; }
  .event-item { background:var(--primary); padding:10px; border-radius:8px; margin:5px 0; display:flex; justify-content:space-between; align-items:center; }
  .remove-btn { background:var(--danger); color:white; border:none; padding:5px 10px; border-radius:4px; cursor:pointer; }
  .add-btn { background:var(--accent); color:var(--bg); }
</style>
</head>
<body>

<h1>🚂 کنترلر یکپارچه ESP32</h1>

<div class="content">

  <div id="motorCard" class="card active">
    <h3>🎛️ کنترل دستی موتور</h3>
    <div id="waterStatus" class="status-bar status-info">وضعیت آب: در حال بررسی...</div>
    <div id="motorStatus" class="status-bar status-info">وضعیت موتور: متوقف</div>
    
    <fieldset id="motorControls">
      <label>جهت</label>
      <div class="btn-group">
        <input type="radio" id="forward" name="direction" value="1" checked style="display:none;">
        <label for="forward" class="btn direction-btn active" onclick="selectDirection(this, 'forward')">جلو</label>
        <input type="radio" id="backward" name="direction" value="0" style="display:none;">
        <label for="backward" class="btn direction-btn" onclick="selectDirection(this, 'backward')">عقب</label>
      </div>
      <label>سرعت</label>
      <div class="btn-group">
        <button class="btn" onclick="setSpeed(100)">آهسته</button>
        <button class="btn" onclick="setSpeed(150)">متوسط</button>
        <button class="btn" onclick="setSpeed(200)">سریع</button>
        <button class="btn" onclick="setSpeed(255)">حداکثر</button>
      </div>
      <div class="btn-group">
        <button class="btn" style="background-color:var(--danger);" onclick="stopMotor()">توقف موتور</button>
      </div>
    </fieldset>
  </div>

  <div id="steamCard" class="card">
    <h3>🌫️ کنترل بخار</h3>
    <div class="btn-group">
      <button class="btn" onclick="setPin15(1)">روشن</button>
      <button class="btn" onclick="setPin15(0)">خاموش</button>
    </div>
    <div id="steamStatus" class="status-bar status-info">وضعیت: خاموش</div>
  </div>

  <div id="musicCard" class="card">
    <h3>🎵 پخش کننده موسیقی</h3>
    <div class="btn-group">
      <button class="btn" onclick="playSong(1)">ترک ۱</button>
      <button class="btn" onclick="playSong(2)">ترک ۲</button>
      <button class="btn" onclick="playSong(3)">ترک ۳</button>
    </div>
    <div class="btn-group">
      <button class="btn" onclick="sendCommand('/pause')">مکث</button>
      <button class="btn" onclick="sendCommand('/stop')">توقف</button>
    </div>
    <label for="volume">میزان صدا</label>
    <input type="range" id="volume" min="0" max="30" value="25" oninput="setVolume()">
    <div id="musicStatus" class="status-bar status-info">وضعیت: متوقف</div>
  </div>

  <div id="autoCard" class="card">
    <h3>🤖 برنامه خودکار</h3>
    <label>مدت زمان کل (دقیقه)</label>
    <input type="number" id="totalDuration" min="1" value="2">
    
    <h4>توقف‌ها</h4>
    <div id="stopList" class="event-list"></div>
    <label>زمان شروع (دقیقه)</label>
    <input type="number" id="stopStart" min="0" step="0.1">
    <label>مدت توقف (ثانیه)</label>
    <input type="number" id="stopDuration" min="1">
    <button class="btn add-btn" onclick="addEvent('stop')">افزودن توقف</button>
    
    <h4>بخار</h4>
    <div id="steamList" class="event-list"></div>
    <label>زمان شروع (دقیقه)</label>
    <input type="number" id="steamStart" min="0" step="0.1">
    <label>مدت (ثانیه)</label>
    <input type="number" id="steamDuration" min="1">
    <button class="btn add-btn" onclick="addEvent('steam')">افزودن بخار</button>
    
    <h4>موسیقی</h4>
    <div id="musicList" class="event-list"></div>
    <label>زمان شروع (دقیقه)</label>
    <input type="number" id="musicStart" min="0" step="0.1">
    <label>شماره ترک (1-3)</label>
    <input type="number" id="musicTrack" min="1" max="3">
    <label>تعداد تکرار</label>
    <input type="number" id="musicRepeats" min="1" value="1">
    <button class="btn add-btn" onclick="addEvent('music')">افزودن موسیقی</button>
    
    <div class="btn-group">
      <button class="btn" onclick="saveProgram()">ذخیره برنامه</button>
      <button class="btn" style="background-color:var(--danger);" onclick="resetDefault()">بازگشت به پیش‌فرض</button>
    </div>
    <div class="btn-group">
      <button class="btn" id="startAutoBtn" onclick="startAuto()">شروع خودکار</button>
      <button class="btn" style="background-color:var(--danger);" id="stopAutoBtn" onclick="stopAuto()">توقف خودکار</button>
    </div>
    <div id="autoStatus" class="status-bar status-info">وضعیت: آماده</div>
  </div>
</div>

<div class="bottom-nav">
  <div class="nav-btn active" onclick="showCard('motorCard', this)">موتور</div>
  <div class="nav-btn" onclick="showCard('steamCard', this)">بخار</div>
  <div class="nav-btn" onclick="showCard('musicCard', this)">موسیقی</div>
  <div class="nav-btn" onclick="showCard('autoCard', this)">خودکار</div>
</div>

<script>
// --- بخش JS برنامه خودکار (از کد ۱) ---
let stops = [];
let steams = [];
let musics = [];

function showCard(cardId, btn){
  document.querySelectorAll('.card').forEach(c => c.classList.remove('active'));
  document.getElementById(cardId).classList.add('active');
  document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
}

function selectDirection(label, directionId) {
    document.querySelectorAll('.direction-btn').forEach(b => b.classList.remove('active'));
    label.classList.add('active');
    document.getElementById(directionId).checked = true;
}

function addEvent(type) {
  let start, duration, track, repeats;
  if (type === 'stop') {
    start = parseFloat(document.getElementById('stopStart').value) * 60000;
    duration = parseInt(document.getElementById('stopDuration').value) * 1000;
    if (isNaN(start) || isNaN(duration)) return;
    stops.push({start, duration});
    renderEvents('stop');
  } else if (type === 'steam') {
    start = parseFloat(document.getElementById('steamStart').value) * 60000;
    duration = parseInt(document.getElementById('steamDuration').value) * 1000;
    if (isNaN(start) || isNaN(duration)) return;
    steams.push({start, duration});
    renderEvents('steam');
  } else if (type === 'music') {
    start = parseFloat(document.getElementById('musicStart').value) * 60000;
    track = parseInt(document.getElementById('musicTrack').value);
    repeats = parseInt(document.getElementById('musicRepeats').value);
    if (isNaN(start) || isNaN(track) || isNaN(repeats)) return;
    musics.push({start, track, repeats});
    renderEvents('music');
  }
}

function renderEvents(type) {
  let listId = type + 'List';
  let list = type === 'stop' ? stops : type === 'steam' ? steams : musics;
  let html = '';
  list.forEach((e, i) => {
    let desc = type === 'music' ? `ترک ${e.track} (تکرار: ${e.repeats}) در دقیقه ${(e.start/60000).toFixed(1)}` : `مدت: ${e.duration/1000} ثانیه در دقیقه ${(e.start/60000).toFixed(1)}`;
    html += `<div class="event-item">${desc} <button class="remove-btn" onclick="removeEvent('${type}', ${i})">حذف</button></div>`;
  });
  document.getElementById(listId).innerHTML = html;
}

function removeEvent(type, index) {
  if (type === 'stop') stops.splice(index, 1);
  else if (type === 'steam') steams.splice(index, 1);
  else if (type === 'music') musics.splice(index, 1);
  renderEvents(type);
}

// --- 🆕 بخش JS کنترل و وضعیت (ادغام شده) ---

// 🆕 توابع فرمان (ساده شده، فقط ارسال درخواست)
// وضعیت به طور خودکار توسط fetchStatus به‌روز می‌شود

function setSpeed(speed) {
  let dir = document.querySelector('input[name="direction"]:checked').value;
  fetch(`/control?speed=${speed}&direction=${dir}`);
}

function stopMotor(){
  fetch('/control'); // بدون پارامتر = توقف
}

function setPin15(status){
  fetch(`/pin15?status=${status}`);
}

function playSong(num){
  fetch(`/play?song=${num}`);
}

function sendCommand(url){
  fetch(url);
}

function setVolume(){
  let v = document.getElementById("volume").value;
  fetch(`/volume?volume=${v}`);
}

async function saveProgram() {
  let total = parseInt(document.getElementById('totalDuration').value) * 60000;
  let url = `/auto_setup?total=${total}`;
  stops.forEach((s, i) => { url += `&stop_start${i}=${s.start}&stop_dur${i}=${s.duration}`; });
  steams.forEach((s, i) => { url += `&steam_start${i}=${s.start}&steam_dur${i}=${s.duration}`; });
  musics.forEach((m, i) => { url += `&music_start${i}=${m.start}&music_track${i}=${m.track}&music_repeats${i}=${m.repeats}`; });
  url += `&num_stops=${stops.length}&num_steams=${steams.length}&num_musics=${musics.length}`;
  
  // برای ذخیره، منتظر پاسخ می‌مانیم و وضعیت را دستی تنظیم می‌کنیم
  try {
    const response = await fetch(url);
    const text = await response.text();
    document.getElementById('autoStatus').innerText = `وضعیت: ${text}`;
  } catch(e) {
    document.getElementById('autoStatus').innerText = "خطا در ذخیره";
  }
}

function startAuto() {
  fetch('/start_auto');
}

function stopAuto() {
  fetch('/stop_auto');
}

async function resetDefault() {
  await fetch('/reset_default');
  stops = []; steams = []; musics = [];
  renderEvents('stop'); renderEvents('steam'); renderEvents('music');
  document.getElementById('totalDuration').value = 2;
  document.getElementById('autoStatus').innerText = "وضعیت: بازگشت به پیش‌فرض";
}

// 🆕 --- قلب تپنده UI جدید: سیستم وضعیت زنده ---
async function fetchStatus() {
  try {
    const response = await fetch('/status');
    const data = await response.json();

    // ۱. به‌روزرسانی کارت موتور
    const motorStatusEl = document.getElementById('motorStatus');
    const waterStatusEl = document.getElementById('waterStatus');
    const controls = document.getElementById('motorControls');
    
    // ۱.الف: وضعیت آب
    waterStatusEl.textContent = `وضعیت آب: ${data.tank_status}`;
    if (data.tank_status === 'خالی') {
        waterStatusEl.className = 'status-bar status-danger';
    } else if (data.tank_status === 'نیمه‌پر') {
        waterStatusEl.className = 'status-bar status-warning';
    } else {
        waterStatusEl.className = 'status-bar status-info';
    }

    // ۱.ب: وضعیت موتور و قفل‌ها
    if (data.motor_locked) {
        motorStatusEl.textContent = 'موتور قفل است: در انتظار پر شدن مخزن';
        motorStatusEl.className = 'status-bar status-danger';
        controls.disabled = true;
    } else if (data.waiting_for_station) {
        motorStatusEl.textContent = 'حرکت: منتظر ایستگاه برای توقف اضطراری';
        motorStatusEl.className = 'status-bar status-warning';
        controls.disabled = false;
    } else if (data.motor_running) {
        const direction = data.motor_direction ? "جلو" : "عقب";
        motorStatusEl.textContent = `موتور روشن (جهت: ${direction}, سرعت: ${data.motor_speed})`;
        motorStatusEl.className = 'status-bar status-info';
        controls.disabled = false;
    } else {
        motorStatusEl.textContent = 'وضعیت موتور: متوقف';
        motorStatusEl.className = 'status-bar status-info';
        controls.disabled = false;
    }
    
    // ۲. به‌روزرسانی کارت بخار
    const steamStatusEl = document.getElementById('steamStatus');
    if(data.steam_active) {
        steamStatusEl.textContent = 'وضعیت: روشن';
        steamStatusEl.className = 'status-bar status-info';
    } else {
        steamStatusEl.textContent = 'وضعیت: خاموش';
        steamStatusEl.className = 'status-bar status-info';
    }
    
    // ۳. به‌روزرسانی کارت موسیقی
    const musicStatusEl = document.getElementById('musicStatus');
    if(data.player_status > 0) { // getPlayingSound()
        musicStatusEl.textContent = `وضعیت: در حال پخش (ترک ${data.player_status})`;
        musicStatusEl.className = 'status-bar status-info';
    } else {
        musicStatusEl.textContent = 'وضعیت: متوقف';
        musicStatusEl.className = 'status-bar status-info';
    }
    
    // ۴. به‌روزرسانی کارت خودکار
    const autoStatusEl = document.getElementById('autoStatus');
    if(data.auto_running) {
        autoStatusEl.textContent = 'وضعیت: در حال اجرا...';
        autoStatusEl.className = 'status-bar status-warning';
    } else {
        autoStatusEl.textContent = 'وضعیت: آماده';
        autoStatusEl.className = 'status-bar status-info';
    }

  } catch (error) {
    console.error("Connection failed:", error);
    // در صورت قطع ارتباط، همه وضعیت‌ها را خطا بزن
    document.getElementById('motorStatus').textContent = 'خطا: قطع ارتباط';
    document.getElementById('motorStatus').className = 'status-bar status-danger';
  }
}

// 🆕 اجرای وضعیت زنده
document.addEventListener('DOMContentLoaded', () => {
    fetchStatus(); // اجرا در لحظه بارگذاری
    setInterval(fetchStatus, 1000); // اجرای هر ۱ ثانیه
});

// استایل دکمه‌های جهت (از کد ۱)
const style = document.createElement('style');
style.innerHTML = `.direction-btn.active { background-color: var(--accent); color: var(--card-bg); }`;
document.head.appendChild(style);
</script>

</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// 🆕 هندلر کنترل موتور (ادغام شده)
// این تابع کنترل دستی را مدیریت می‌کند و به قفل احترام می‌گذارد
void handleControl() {
  // اگر موتور قفل است، هیچ فرمان دستی اجرا نشود
  if (motorLockedDueToEmptyTank) {
    stopMotor(); // اطمینان از توقف
    server.send(200, "text/plain", "Motor Locked (Tank Empty)");
    return;
  }
  
  // اگر برنامه خودکار روشن است، کنترل دستی کار نکند
  if (autoRunning) {
    server.send(200, "text/plain", "Auto program is running");
    return;
  }

  if (server.hasArg("speed") && server.hasArg("direction")) {
    currentSpeed = server.arg("speed").toInt();
    currentDirection = server.arg("direction").toInt() == 1;
    
    startMotor(); // استفاده از تابع جدید که قفل را چک می‌کند
    
    String directionStr = currentDirection ? "Forward" : "Backward";
    server.send(200, "text/plain", "Running " + directionStr + " at " + String(currentSpeed));
  } else {
    // توقف دستی
    stopMotor();
    server.send(200, "text/plain", "Stopped");
  }
}

// هندلر بخار (پین ۱۵)
void handlePin15() {
  if (server.hasArg("status")) {
    bool pinStatus = server.arg("status").toInt() == 1;
    digitalWrite(STEAM_PIN, pinStatus ? HIGH : LOW);
    steamActive = pinStatus; // 🆕 به‌روزرسانی وضعیت سراسری
    server.send(200, "text/plain", pinStatus ? "ON" : "OFF");
  }
}

// هندلرهای موسیقی (از کد ۱ - با کمی تغییر)
void handlePlay() {
  if (autoRunning) return; // عدم دخالت در حالت خودکار
  if (server.hasArg("song")) {
    int songNumber = server.arg("song").toInt();
    player.setCycleMode(DY::PlayMode::RepeatOne);
    player.playSpecified(songNumber);
    server.send(200, "text/plain", "Playing Track " + String(songNumber));
  } else {
    player.play();
    server.send(200, "text/plain", "Resumed");
  }
}

void handlePause() {
  if (autoRunning) return;
  player.pause();
  server.send(200, "text/plain", "Paused");
}

void handleStop() {
  if (autoRunning) return;
  player.stop();
  player.setCycleMode(DY::PlayMode::OneOff);
  server.send(200, "text/plain", "Stopped");
}

void handleVolume() {
  if (server.hasArg("volume")) {
    int volume = server.arg("volume").toInt();
    player.setVolume(volume);
    server.send(200, "text/plain", "Volume set to " + String(volume));
  }
}

// هندلرهای برنامه خودکار (از کد ۱)
void handleAutoSetup() {
  totalDuration = server.arg("total").toInt();
  numStops = server.arg("num_stops").toInt();
  for (int i = 0; i < numStops; i++) {
    stops[i].startTime = server.arg("stop_start" + String(i)).toInt();
    stops[i].duration = server.arg("stop_dur" + String(i)).toInt();
  }
  numSteams = server.arg("num_steams").toInt();
  for (int i = 0; i < numSteams; i++) {
    steams[i].startTime = server.arg("steam_start" + String(i)).toInt();
    steams[i].duration = server.arg("steam_dur" + String(i)).toInt();
  }
  numMusics = server.arg("num_musics").toInt();
  for (int i = 0; i < numMusics; i++) {
    musics[i].startTime = server.arg("music_start" + String(i)).toInt();
    musics[i].songNumber = server.arg("music_track" + String(i)).toInt();
    musics[i].repeats = server.arg("music_repeats" + String(i)).toInt();
    musics[i].currentRepeat = 0;
    musics[i].playing = false;
  }
  server.send(200, "text/plain", "برنامه ذخیره شد");
}

void handleStartAuto() {
  if (!autoRunning) {
    // 🛑 قبل از شروع، چک کن موتور قفل نباشد
    if (motorLockedDueToEmptyTank) {
      server.send(200, "text/plain", "خطا: مخزن خالی است!");
      return;
    }
    stopAll(); // ریست کردن همه‌چیز قبل از شروع
    autoRunning = true;
    autoStartTime = millis();
    // startMotor(); // موتور در حلقه loop خودکار روشن می‌شود
    server.send(200, "text/plain", "Auto Started");
  } else {
    server.send(200, "text/plain", "Already Running");
  }
}

void handleStopAuto() {
  stopAll();
  autoRunning = false;
  server.send(200, "text/plain", "Auto Stopped");
}

void handleResetDefault() {
  setDefaultProgram();
  server.send(200, "text/plain", "Reset to Default");
}

// 🆕 --- هندلر جدید برای وضعیت زنده ---
// این تابع یک JSON با تمام اطلاعات سیستم ارسال می‌کند
void handleStatus() {
  // وضعیت پخش‌کننده موسیقی را دریافت کن
  int playerStatus = player.getPlayingSound();

  // ساخت رشته JSON
  String json = "{";
  json += "\"motor_running\":" + String(motorRunning);
  json += ",\"motor_direction\":" + String(currentDirection);
  json += ",\"motor_speed\":" + String(motorRunning ? currentSpeed : 0);
  json += ",\"steam_active\":" + String(steamActive);
  json += ",\"tank_status\":\"" + String(tankStatus) + "\"";
  json += ",\"motor_locked\":" + String(motorLockedDueToEmptyTank);
  json += ",\"waiting_for_station\":" + String(waitingForRefillStop);
  json += ",\"auto_running\":" + String(autoRunning);
  json += ",\"player_status\":" + String(playerStatus);
  json += "}";
  
  server.send(200, "application/json", json);
}