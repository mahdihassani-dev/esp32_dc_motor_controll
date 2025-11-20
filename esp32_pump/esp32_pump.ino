#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <ESP32Servo.h>

// --- تنظیمات سخت‌افزار ---
const int SERVO_PIN = 27;      // پین سروو
const int PUMP_IN1 = 18;       // پین 1 پمپ
const int PUMP_IN2 = 19;       // پین 2 پمپ

// --- تنظیمات زوایای سروو ---
const int ANGLE_HOME = 0;      
const int ANGLE_FILL = 90;     

const int CUSTOM_DELAY = 3000;

// --- MAC Address قطار ---
uint8_t trainMacAddress[] = {0xFC, 0xE8, 0xC0, 0x7D, 0xD4, 0x90}; 

// ساختار پیام
typedef struct {
  char command[20];
} Message;

// متغیرهای سراسری
Servo myServo;
WebServer server(80);

// متغیرهای وضعیت برای نمایش در وب
String uiStatusText = "آماده به کار";
bool uiPumpActive = false;
int uiArmAngle = ANGLE_HOME;
bool fillingProcessActive = false;

// --- توابع کنترل سخت‌افزار ---

void stopPump() {
  digitalWrite(PUMP_IN1, LOW);
  digitalWrite(PUMP_IN2, LOW);
  uiPumpActive = false; // آپدیت وضعیت وب
  Serial.println("Pump STOPPED");
}

void startPump() {
  digitalWrite(PUMP_IN1, HIGH);
  digitalWrite(PUMP_IN2, LOW);
  uiPumpActive = true; // آپدیت وضعیت وب
  Serial.println("Pump STARTED");
}

void moveArm(int angle) {
  myServo.write(angle);
  uiArmAngle = angle; // آپدیت وضعیت وب
  Serial.print("Servo moved to: ");
  Serial.println(angle);
}

// ارسال پیام به قطار
void sendToTrain(const char* cmd) {
  Message msg;
  strcpy(msg.command, cmd);
  esp_now_send(trainMacAddress, (uint8_t *) &msg, sizeof(msg));
}

// --- مدیریت درخواست‌های وب ---

// 1. ارسال صفحه اصلی HTML (زیبا و مینیمال)
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ایستگاه هوشمند</title>
<style>
  :root { --bg: #1e1e2e; --card: #2a2a3c; --text: #fff; --accent: #4a90e2; --danger: #ff5555; --success: #50fa7b; }
  body { background-color: var(--bg); color: var(--text); font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
  .container { background-color: var(--card); padding: 30px; border-radius: 20px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); width: 90%; max-width: 400px; text-align: center; }
  h1 { margin-bottom: 5px; font-size: 1.5rem; color: var(--accent); }
  .status-text { font-size: 1.2rem; margin: 20px 0; font-weight: bold; min-height: 30px; color: #ffd700; }
  
  /* گرافیک‌ها */
  .indicators { display: flex; justify-content: space-around; margin: 30px 0; }
  .indicator { display: flex; flex-direction: column; align-items: center; }
  .circle { width: 60px; height: 60px; border-radius: 50%; background: #444; display: flex; justify-content: center; align-items: center; transition: all 0.3s ease; font-size: 24px; }
  
  /* حالت فعال پمپ */
  .pump.active { background: var(--accent); box-shadow: 0 0 15px var(--accent); animation: pulse 1s infinite; }
  @keyframes pulse { 0% { transform: scale(1); } 50% { transform: scale(1.1); } 100% { transform: scale(1); } }
  
  /* حالت فعال بازو */
  .arm-bar { width: 10px; height: 40px; background: #fff; border-radius: 5px; transition: transform 1s ease; transform-origin: bottom center; }
  
  .label { margin-top: 10px; font-size: 0.9rem; opacity: 0.8; }
  .footer { margin-top: 20px; font-size: 0.8rem; color: #666; }
</style>
</head>
<body>
  <div class="container">
    <h1>⛽ ایستگاه سوخت‌گیری</h1>
    <div id="status" class="status-text">در حال دریافت اطلاعات...</div>
    
    <div class="indicators">
      <div class="indicator">
        <div id="pumpIcon" class="circle pump">💧</div>
        <span class="label">پمپ آب</span>
      </div>
      
      <div class="indicator">
        <div class="circle">
          <div id="armIcon" class="arm-bar"></div>
        </div>
        <span class="label">بازو رباتیک</span>
      </div>
    </div>
    
    <div class="footer">IP: <span id="ip">...</span></div>
  </div>

<script>
  function updateStatus() {
    fetch('/status')
      .then(response => response.json())
      .then(data => {
        // آپدیت متن
        document.getElementById('status').innerText = data.msg;
        
        // آپدیت پمپ
        const pumpEl = document.getElementById('pumpIcon');
        if(data.pump) pumpEl.classList.add('active');
        else pumpEl.classList.remove('active');
        
        // آپدیت بازو (چرخش بر اساس زاویه)
        // زاویه 0 = بالا (صاف)، زاویه 90 = پایین (چرخش به راست)
        // ما ترنسفورم CSS رو تنظیم می‌کنیم
        let visualAngle = data.angle === 90 ? 90 : 0; 
        document.getElementById('armIcon').style.transform = `rotate(${visualAngle}deg)`;
      })
      .catch(e => console.log(e));
  }

  // نمایش IP
  document.getElementById('ip').innerText = window.location.hostname;

  // آپدیت هر 1 ثانیه
  setInterval(updateStatus, 1000);
  updateStatus();
</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// 2. ارسال داده‌های JSON برای آپدیت زنده
void handleStatus() {
  String json = "{";
  json += "\"msg\":\"" + uiStatusText + "\",";
  json += "\"pump\":" + String(uiPumpActive ? "true" : "false") + ",";
  json += "\"angle\":" + String(uiArmAngle);
  json += "}";
  server.send(200, "application/json", json);
}

// --- Callback دریافت پیام ---
// برای اینکه وب‌سرور هنگ نکند، بهتر است منطق تاخیردار را مدیریت کنیم
// اما چون منطق شما ساده است، آپدیت متن UI را دقیقا قبل از delay انجام می‌دهیم

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  Message msg;
  memcpy(&msg, incomingData, sizeof(msg));
  String cmd = String(msg.command);
  Serial.print("Received: "); Serial.println(cmd);

  if (cmd == "start_filling") {
    if (!fillingProcessActive) {
      fillingProcessActive = true;
      
      uiStatusText = "قطار رسید. تثبیت...";
      // هندل کردن کلاینت وب قبل از توقف پردازنده
      // نکته: delay باعث فریز شدن موقت وب می‌شود، اما چون ما قبلش
      // متغیرها را ست کردیم، در درخواست بعدی وب‌سرور (بعد از delay) کاربر آپدیت را می‌بیند.
      
      delay(CUSTOM_DELAY); 
      
      uiStatusText = "حرکت بازو به داخل...";
      moveArm(ANGLE_FILL);
      delay(CUSTOM_DELAY); 
      
      uiStatusText = "شروع پمپاژ آب...";
      startPump();
    }
  } 
  else if (cmd == "stop_filling") {
    if (fillingProcessActive) {
      uiStatusText = "مخزن پر شد. توقف...";
      stopPump();
      
      uiStatusText = "چکیدن قطرات...";
      delay(CUSTOM_DELAY); 
      
      uiStatusText = "جمع کردن بازو...";
      moveArm(ANGLE_HOME);
      
      Serial.println("Arm Home. Telling train to GO.");
      delay(CUSTOM_DELAY); 
      
      sendToTrain("train_go"); 
      
      uiStatusText = "عملیات تمام شد. آماده.";
      fillingProcessActive = false;
    }
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

// --- Setup & Loop ---
void setup() {
  Serial.begin(115200);
  
  pinMode(PUMP_IN1, OUTPUT);
  pinMode(PUMP_IN2, OUTPUT);
  stopPump();
  
  myServo.attach(SERVO_PIN);
  moveArm(ANGLE_HOME);
  
  // WiFi Setup
  WiFi.mode(WIFI_STA);
  const char* ssid = "Esp32";
  const char* password = "1234567876543212";
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  Serial.println("\nWeb Server ready at: ");
  Serial.println(WiFi.localIP());

  // ESP-NOW
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, trainMacAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Web Server Routes
  server.on("/", HTTP_GET, handleRoot);      // صفحه اصلی
  server.on("/status", HTTP_GET, handleStatus); // دیتای زنده JSON
  server.begin();
}

void loop() {
  server.handleClient();
}