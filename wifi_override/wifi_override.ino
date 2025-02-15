#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

// Pins
const int brightPotPin = 34;
const int huePotPin = 35;
const int redLedPin = 25;
const int greenLedPin = 26;
const int buttonPin = 2;
const int fakeGND = 15; // Using GPIO 15 as fake GND

// WiFi Credentials
const char* ssid = "iPhone";
const char* password = "12345678";

// Web Server
WebServer server(80);

// State Variables
bool manualOverride = false;  // false = web control, true = manual control
int totalBrightness = 0;
int hueRatio = 0;
int webBrightness = 128;     // Renamed for clarity
int webHueRatio = 50;        // Renamed for clarity

// Debounce
volatile unsigned long lastDebounceTime = 0;

// Transition Variables
bool isTransitioning = false;
unsigned long transitionStartTime = 0;
const unsigned long TRANSITION_DURATION = 20000; // 20 seconds

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
  pinMode(fakeGND, OUTPUT);
  digitalWrite(fakeGND, LOW); // bc ground broke:/

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
  server.on("/startTransition", handleStartTransition);
  server.begin();
}

void loop() {
  server.handleClient();
  // Only allow transitions in web mode
  if (isTransitioning && !manualOverride) {
    updateTransition();
  } else {
    updateLEDs();
  }
}

// Interrupt Handler
void IRAM_ATTR toggleOverride() {
  if (millis() - lastDebounceTime > 200) {
    manualOverride = !manualOverride;
    isTransitioning = false;  // Stop any transition when mode changes
    lastDebounceTime = millis();
    Serial.println(manualOverride ? "Manual mode enabled" : "Web mode enabled");
  }
}

void updateLEDs() {
  if (manualOverride) {
    // Manual mode - read from potentiometers
    totalBrightness = analogRead(brightPotPin) >> 2;
    hueRatio = analogRead(huePotPin) / 10.23;
  } else {
    // Web mode - use web values
    totalBrightness = webBrightness;
    hueRatio = webHueRatio;
  }

  int red = map(hueRatio, 0, 100, 0, totalBrightness);
  int green = totalBrightness - red;
  analogWrite(redLedPin, red);
  analogWrite(greenLedPin, green);
}

void updateTransition() {
  unsigned long elapsedTime = millis() - transitionStartTime;
  if (elapsedTime <= TRANSITION_DURATION) {
    float progress = (float)elapsedTime / TRANSITION_DURATION;
    int red, green;
    if (progress <= 0.33) {
      red = (progress / 0.33) * 255;
      green = 0;
    } else if (progress <= 0.66) {
      red = 255 - ((progress - 0.33) / 0.33) * 255;
      green = ((progress - 0.33) / 0.33) * 255;
    } else {
      red = 0;
      green = 255 - ((progress - 0.66) / 0.34) * 255;
    }
    analogWrite(redLedPin, red);
    analogWrite(greenLedPin, green);
  } else {
    isTransitioning = false;
    updateLEDs();
  }
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleSet() {
  if (manualOverride) {
    server.send(403, "text/plain", "Manual override active");
    return;
  }

  if (server.hasArg("brightness")) {
    webBrightness = server.arg("brightness").toInt();
  }
  if (server.hasArg("hue")) {
    webHueRatio = server.arg("hue").toInt();
  }
  server.send(200, "text/plain", "OK");
}

void handleStartTransition() {
  if (manualOverride) {
    server.send(403, "text/plain", "Manual override active");
    return;
  }

  isTransitioning = true;
  transitionStartTime = millis();
  server.send(200, "text/plain", "Transition started");
}

String getHTML() {
  return R"html(
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
          text-align: center;
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
        .button {
          margin-top: 20px;
          padding: 12px 20px;
          font-size: 16px;
          color: white;
          background: #4CAF50;
          border: none;
          border-radius: 5px;
          cursor: pointer;
          transition: background 0.3s;
        }
        .button:hover {
          background: #45a049;
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

      <button class="button" onclick="startTransition()">Start Transition</button>

      <script>
        function sendUpdate(type, value) {
          const xhr = new XMLHttpRequest();
          xhr.open("GET", `/set?${type}=${value}`, true);
          xhr.onreadystatechange = function () {
            if (xhr.readyState === 4 && xhr.status === 403) {
              alert("Manual mode active. Web controls disabled.");
            }
          };
          xhr.send();
        }

        function startTransition() {
          const xhr = new XMLHttpRequest();
          xhr.open("GET", "/startTransition", true);
          xhr.onreadystatechange = function () {
            if (xhr.readyState === 4 && xhr.status === 403) {
              alert("Manual mode active. Transition blocked.");
            }
          };
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
  )html";
}