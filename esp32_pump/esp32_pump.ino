#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>

// --- MAC address این ESP (برای چاپ) ---
uint8_t myMacAddress[6]; // بعداً پر می‌شه

// struct برای پیام ESP-NOW (همانند ESP اول)
typedef struct {
  char command[20];
} Message;

// متغیر برای ذخیره پیام دریافتی (برای نمایش در وب)
String receivedMessage = "هیچ پیامی دریافت نشده";

// callback برای دریافت ESP-NOW
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  Message msg;
  memcpy(&msg, incomingData, sizeof(msg));
  receivedMessage = String("پیام دریافتی: ") + msg.command;
  Serial.println(receivedMessage);
  // اینجا در آینده: اگر "start_filling" باشه، پمپ رو روشن کن؛ اگر "stop_filling"، خاموش کن.
}

// تنظیمات شبکه
const char* ssid = "Esp32";
const char* password = "1234567876543212";
WebServer server(80);

void setup() {
  Serial.begin(115200);
  
  // اتصال به WiFi (لازم برای ESP-NOW)
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA); // حالت Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // برای دسترسی به وب‌سرور
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress()); // این رو کپی کنید و در ESP اول بگذارید

  // مقداردهی ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  // ثبت callback دریافت
  esp_now_register_recv_cb(OnDataRecv);

  // راه‌اندازی وب‌سرور ساده برای نمایش پیام
  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="UTF-8">
<title>ایستگاه پر کردن</title>
<style>
body { font-family: sans-serif; text-align: center; margin-top: 50px; }
#message { font-size: 1.5em; color: #333; }
</style>
</head>
<body>
<h1>وضعیت ایستگاه</h1>
<p id="message">در حال بارگذاری...</p>
<script>
function fetchMessage() {
  fetch('/message')
    .then(response => response.json())
    .then(data => {
      document.getElementById('message').innerText = data.message;
    })
    .catch(error => {
      console.error('Error:', error);
      document.getElementById('message').innerText = 'خطا در دریافت پیام';
    });
}
fetchMessage(); // بارگذاری اولیه
setInterval(fetchMessage, 1000); // به‌روزرسانی هر 1 ثانیه
</script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
  });

  // 🆕 مسیر جدید برای دریافت پیام به صورت JSON (برای polling)
  server.on("/message", HTTP_GET, []() {
    String json = "{\"message\": \"" + receivedMessage + "\"}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Web server started. Access at http://" + WiFi.localIP().toString() + "/");
}

void loop() {
  server.handleClient();
  // اینجا در آینده: کنترل پمپ و بازو بر اساس receivedMessage
}