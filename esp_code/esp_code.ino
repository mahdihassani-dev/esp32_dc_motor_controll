#include <WiFi.h>
#include <WebServer.h>

// Motor Control Pins
const int ENA = 14;  // Speed control pin
const int IN1 = 27;  // Direction control pin 1
const int IN2 = 26;  // Direction control pin 2

// WiFi credentials
const char* ssid = "mahdi1383";    // Replace with your network SSID
const char* password = "mahdi&mohammad83";  // Replace with your network password

// Create a web server object that listens for HTTP requests on port 80
WebServer server(80);

// Variables to hold motor status
int motorSpeed = 0;  // Speed from 0 to 255
bool motorRunning = false;  // Motor start/stop
bool motorDirection = true; // true for forward, false for backward

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set up motor control pins
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  // Initialize motor as stopped
  digitalWrite(ENA, LOW);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  // Connect to Wi-Fi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected!");

  // Print the ESP32 IP Address
  Serial.println(WiFi.localIP());

  // Set up the web server routes
  server.on("/", handleRoot);  // Home page
  server.on("/control", handleControl);  // Motor control

  // Start the server
  server.begin();
}

void loop() {
  // Handle any incoming client requests
  server.handleClient();
}

// Handle the root page
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESP32 Motor Control</h1>";
  html += "<p>Motor Status: " + String(motorRunning ? "Running" : "Stopped") + "</p>";
  html += "<p>Motor Direction: " + String(motorDirection ? "Forward" : "Backward") + "</p>";
  html += "<p>Speed: " + String(motorSpeed) + "</p>";
  html += "<form action=\"/control\">";
  html += "Speed (0-255): <input type=\"number\" name=\"speed\"><br>";
  html += "Direction: <input type=\"radio\" name=\"direction\" value=\"1\" checked> Forward";
  html += "<input type=\"radio\" name=\"direction\" value=\"0\"> Backward<br>";
  html += "<input type=\"submit\" value=\"Start Motor\">";
  html += "</form>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Handle motor control
void handleControl() {
  if (server.hasArg("speed")) {
    motorSpeed = server.arg("speed").toInt();
    motorRunning = true;

    // Set motor speed
    analogWrite(ENA, motorSpeed);

    // Set motor direction
    motorDirection = server.arg("direction") == "1";
    digitalWrite(IN1, motorDirection ? HIGH : LOW);
    digitalWrite(IN2, motorDirection ? LOW : HIGH);

    server.send(200, "text/html", "<p>Motor started</p><a href=\"/\">Back</a>");
  } else {
    motorRunning = false;
    analogWrite(ENA, 0);  // Stop motor
    server.send(200, "text/html", "<p>Motor stopped</p><a href=\"/\">Back</a>");
  }
}
