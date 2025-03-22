#include <WiFi.h>
#include <WebServer.h>
#include "DYPlayerArduino.h"

// Motor Control Pins
const int ENA = 14;  // Speed control pin
const int IN1 = 27;  // Direction control pin 1
const int IN2 = 26;  // Direction control pin 2
const int PIN15 = 15;  // Pin 15 control

// WiFi credentials
const char* ssid = "Esp32";    // Replace with your network SSID
const char* password = "1234567876543212";  // Replace with your network password

// Create a web server object that listens for HTTP requests on port 80
WebServer server(80);

// Variables to hold motor status
int motorSpeed = 0;  // Speed from 0 to 255
bool motorRunning = false;  // Motor start/stop
bool motorDirection = true; // true for forward, false for backward
bool pin15Status = false;   // Pin 15 status

// DY Player object
DY::Player player(&Serial2);

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set up motor control pins
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(PIN15, OUTPUT);

  // Initialize motor as stopped
  digitalWrite(ENA, LOW);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(PIN15, LOW);

  // Initialize DY Player
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  player.begin();
  player.setCycleMode(DY::PlayMode::OneOff);
  player.setVolume(30); // Default volume

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
  server.on("/pin15", handlePin15);  // Pin 15 control
  server.on("/play", handlePlay);  // Play sound
  server.on("/pause", handlePause);  // Pause sound
  server.on("/stop", handleStop);  // Stop sound
  server.on("/volume", handleVolume);  // Set volume

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
  html += "<h1>ESP32 Motor and Sound Control</h1>";
  html += "<p>Motor Status: " + String(motorRunning ? "Running" : "Stopped") + "</p>";
  html += "<p>Motor Direction: " + String(motorDirection ? "Forward" : "Backward") + "</p>";
  html += "<p>Speed: " + String(motorSpeed) + "</p>";
  html += "<p>Pin 15 Status: " + String(pin15Status ? "ON" : "OFF") + "</p>";
  html += "<form action=\"/control\">";
  html += "Speed (0-255): <input type=\"number\" name=\"speed\"><br>";
  html += "Direction: <input type=\"radio\" name=\"direction\" value=\"1\" checked> Forward";
  html += "<input type=\"radio\" name=\"direction\" value=\"0\"> Backward<br>";
  html += "<input type=\"submit\" value=\"Start Motor\">";
  html += "</form>";
  html += "<form action=\"/pin15\">";
  html += "Pin 15: <input type=\"radio\" name=\"status\" value=\"1\"> ON";
  html += "<input type=\"radio\" name=\"status\" value=\"0\"> OFF<br>";
  html += "<input type=\"submit\" value=\"Set Pin 15\">";
  html += "</form>";
  html += "<h2>Sound Control</h2>";
  html += "<form action=\"/play\">";
  html += "Song Number: <input type=\"number\" name=\"song\"><br>";
  html += "<input type=\"submit\" value=\"Play\">";
  html += "</form>";
  html += "<form action=\"/pause\">";
  html += "<input type=\"submit\" value=\"Pause\">";
  html += "</form>";
  html += "<form action=\"/stop\">";
  html += "<input type=\"submit\" value=\"Stop\">";
  html += "</form>";
  html += "<form action=\"/volume\">";
  html += "Volume (0-30): <input type=\"number\" name=\"volume\"><br>";
  html += "<input type=\"submit\" value=\"Set Volume\">";
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

// Handle Pin 15 control
void handlePin15() {
  if (server.hasArg("status")) {
    pin15Status = server.arg("status") == "1";
    digitalWrite(PIN15, pin15Status ? HIGH : LOW);
    server.send(200, "text/html", "<p>Pin 15 " + String(pin15Status ? "ON" : "OFF") + "</p><a href=\"/\">Back</a>");
  }
}

// Handle play sound
void handlePlay() {
  if (server.hasArg("song")) {
    int songNumber = server.arg("song").toInt();
    if (songNumber > 0) {
      player.playSpecified(songNumber);
      server.send(200, "text/html", "<p>Playing song " + String(songNumber) + "</p><a href=\"/\">Back</a>");
    } else {
      server.send(400, "text/html", "<p>Invalid song number</p><a href=\"/\">Back</a>");
    }
  }
}

// Handle pause sound
void handlePause() {
  player.pause();
  server.send(200, "text/html", "<p>Sound paused</p><a href=\"/\">Back</a>");
}

// Handle stop sound
void handleStop() {
  player.stop();
  server.send(200, "text/html", "<p>Sound stopped</p><a href=\"/\">Back</a>");
}

// Handle set volume
void handleVolume() {
  if (server.hasArg("volume")) {
    int volume = server.arg("volume").toInt();
    if (volume >= 0 && volume <= 30) {
      player.setVolume(volume);
      server.send(200, "text/html", "<p>Volume set to " + String(volume) + "</p><a href=\"/\">Back</a>");
    } else {
      server.send(400, "text/html", "<p>Invalid volume</p><a href=\"/\">Back</a>");
    }
  }
}