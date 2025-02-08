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

// Interrupt Handler
void IRAM_ATTR toggleOverride() {
  if (millis() - lastDebounceTime > 200) {
    manualOverride = !manualOverride;
    lastDebounceTime = millis();
  }
}

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

// Web Page HTML
void handleRoot() {
  String html = R"(
    <!DOCTYPE html>
    <html>
    <body>
      <h1>LED Control</h1>
      <form action="/set">
        Brightness (0-255): <input type="number" name="brightness" min="0" max="255" value=")" + String(manualBrightness) + R"(">
        Hue (0-100): <input type="number" name="hue" min="0" max="100" value=")" + String(manualHueRatio) + R"(">
        <input type="submit" value="Update">
      </form>
    </body>
    </html>
  )";
  server.send(200, "text/html", html);
}

// Handle Web Input
void handleSet() {
  if (server.hasArg("brightness") && server.hasArg("hue")) {
    manualBrightness = server.arg("brightness").toInt();
    manualHueRatio = server.arg("hue").toInt();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}