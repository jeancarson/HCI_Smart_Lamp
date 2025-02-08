#include <WiFi.h>
#include <WebServer.h>

// Pins
const int brightPotPin = 34;
const int huePotPin = 35;
const int redLedPin = 25;
const int greenLedPin = 26;
const int buttonPin = 2;

// WiFi Credentials
const char* ssid = "iPhone";
const char* password = "12345678";

// Web Server
WebServer server(80);

// State Variables
bool manualOverride = false;
int totalBrightness = 0;
int hueRatio = 0;
int manualBrightness = 128;
int manualHueRatio = 50;

// Debounce
volatile unsigned long lastDebounceTime = 0;

// Function Prototype
void IRAM_ATTR toggleOverride();

void setup() {
  Serial.begin(115200);
  
  // Pin Setup
  pinMode(brightPotPin, INPUT);
  pinMode(huePotPin, INPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // Interrupt for Override Button
  attachInterrupt(digitalPinToInterrupt(buttonPin), toggleOverride, FALLING);

  // WiFi Setup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Web Server Routes
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
}

void loop() {
  server.handleClient();

  if (!manualOverride) {
    // Read Pots in Normal Mode
    totalBrightness = analogRead(brightPotPin) >> 2;  // 0-1023 to 0-255
    hueRatio = analogRead(huePotPin) / 10.23;         // 0-1023 to 0-100
  } else {
    // Use Web Values in Override
    totalBrightness = manualBrightness;
    hueRatio = manualHueRatio;
  }

  // Update LEDs
  int red = map(hueRatio, 0, 100, 0, totalBrightness);
  int green = totalBrightness - red;
  analogWrite(redLedPin, red);
  analogWrite(greenLedPin, green);
}

// Interrupt Handler
void IRAM_ATTR toggleOverride() {
  if (millis() - lastDebounceTime > 200) {
    manualOverride = !manualOverride;
    lastDebounceTime = millis();
  }
}

// Serve the HTML file
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// Handle Web Input
void handleSet() {
  if (server.hasArg("brightness")) {
    manualBrightness = server.arg("brightness").toInt();
  }
  if (server.hasArg("hue")) {
    manualHueRatio = server.arg("hue").toInt();
  }
  server.send(200, "text/plain", "OK");
}

// Get the HTML content
String getHTML() {
  return R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>LED Control</title>
      <style>
        body {
          background-color: #1a1a1a;
          color: white;
          font-family: monospace;
          margin: 0;
          padding: 20px;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          height: 100vh;
        }
        h1 {
          margin-bottom: 20px;
        }
        .slider-container {
          width: 80%;
          max-width: 300px;
          margin: 10px 0;
        }
        .slider {
          width: 100%;
          height: 15px;
          border-radius: 5px;
          background: #333;
          outline: none;
          opacity: 0.7;
          transition: opacity 0.2s;
        }
        .slider:hover {
          opacity: 1;
        }
        .slider::-webkit-slider-thumb {
          -webkit-appearance: none;
          appearance: none;
          width: 25px;
          height: 25px;
          border-radius: 50%;
          background: #4CAF50;
          cursor: pointer;
        }
        .slider::-moz-range-thumb {
          width: 25px;
          height: 25px;
          border-radius: 50%;
          background: #4CAF50;
          cursor: pointer;
        }
        label {
          display: block;
          margin-bottom: 5px;
          font-size: 16px;
        }
      </style>
    </head>
    <body>
      <h1>Set your settings</h1>
      <div class="slider-container">
        <label for="brightness">Brightness:</label>
        <input type="range" id="brightness" class="slider" min="0" max="255" value="128">
      </div>
      <div class="slider-container">
        <label for="hue">Hue:</label>
        <input type="range" id="hue" class="slider" min="0" max="100" value="50">
      </div>

      <script>
        function sendUpdate(type, value) {
          const xhr = new XMLHttpRequest();
          xhr.open("GET", `/set?${type}=${value}`, true);
          xhr.send();
        }

        document.getElementById("brightness").addEventListener("input", function() {
          sendUpdate("brightness", this.value);
        });

        document.getElementById("hue").addEventListener("input", function() {
          sendUpdate("hue", this.value);
        });
      </script>
    </body>
    </html>
  )";
}