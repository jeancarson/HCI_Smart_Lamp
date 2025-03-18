#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <Preferences.h>
#include <time.h>

// Pins
#define LED_PIN         5
#define NUM_LEDS        60
#define BRIGHT_POT_PIN  A2
#define HUE_POT_PIN     A4
#define MODE_SWITCH_PIN 9
#define BUTTON_PIN      10

// WiFi Credentials
const char* ssid = "Fred's A70";
const char* password = "password05";

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

// Color Definitions
CRGB leds[NUM_LEDS];
CRGB warmColor = CRGB(255, 150, 100);
CRGB coldColor = CRGB(150, 180, 255);

// Web Server
WebServer server(80);
Preferences preferences;

// State Variables
volatile bool powerOn = false;
int webBrightness = 128;
struct Schedule {
  int startHour;
  int startMinute;
  int endHour;
  int endMinute;
} currentSchedule;

// Debounce
volatile unsigned long lastDebounceTime = 0;

void IRAM_ATTR togglePower() {
  if (millis() - lastDebounceTime > 200) {
    powerOn = !powerOn;
    lastDebounceTime = millis();
    Serial.println(powerOn ? "Power ON" : "Power OFF");
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize LEDs
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.show();

  // Initialize hardware
  pinMode(BRIGHT_POT_PIN, INPUT);
  pinMode(HUE_POT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), togglePower, FALLING);

  // Load saved schedule
  loadSchedule();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Configure time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Setup web server
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/setSchedule", handleSetSchedule);
  server.begin();
}

void loop() {
  server.handleClient();
  updateLEDs();
  delay(500);  // This is the tick length - LEDs will update every 500ms
}

void loadSchedule() {
  preferences.begin("schedule", true);
  currentSchedule.startHour = preferences.getInt("startH", 8);
  currentSchedule.startMinute = preferences.getInt("startM", 0);
  currentSchedule.endHour = preferences.getInt("endH", 17);
  currentSchedule.endMinute = preferences.getInt("endM", 0);
  preferences.end();
}

void saveSchedule() {
  preferences.begin("schedule", false);
  preferences.putInt("startH", currentSchedule.startHour);
  preferences.putInt("startM", currentSchedule.startMinute);
  preferences.putInt("endH", currentSchedule.endHour);
  preferences.putInt("endM", currentSchedule.endMinute);
  preferences.end();
}

void printColor(CRGB* color) {
  Serial.print("Color: (");
  Serial.print(color->r);
  Serial.print(", ");
  Serial.print(color->g);
  Serial.print(", ");
  Serial.print(color->b);
  Serial.println(")");
}

void updateLEDsFromCircuit() {
    int brightness = analogRead(BRIGHT_POT_PIN) >> 4; // Convert 12-bit ADC to 8-bit (0-4096 -> 0-255)
    int roundedBrightness = ((brightness + 5) / 10) * 10; // Round to nearest 10
    if (roundedBrightness > 255) roundedBrightness = 255;
    Serial.println("Brightness: "); 
    Serial.println(roundedBrightness);
    int hueValue = analogRead(HUE_POT_PIN); // 0-100
    Serial.print("Hue: ");
    Serial.println(hueValue);

    CRGB color = blend(warmColor, coldColor, hueValue * 2.55);
    FastLED.setBrightness(roundedBrightness);
    fill_solid(leds, NUM_LEDS, color);
}

float calculateProgress(int currentSeconds, int startSeconds, int endSeconds) {
    int totalDuration = (startSeconds < endSeconds) ?
        (endSeconds - startSeconds) :
        ((86400 - startSeconds) + endSeconds);

    if (totalDuration == 0) totalDuration = 1;

    int elapsed = (currentSeconds >= startSeconds) ?
        (currentSeconds - startSeconds) :
        ((86400 - startSeconds) + currentSeconds);

    return constrain((float)elapsed / totalDuration, 0.0, 1.0);
}

void updateLEDsFromWeb() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    int currentSeconds = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    int startSeconds = currentSchedule.startHour * 3600 + currentSchedule.startMinute * 60;
    int endSeconds = currentSchedule.endHour * 3600 + currentSchedule.endMinute * 60;

    bool isActive = (startSeconds < endSeconds) ?
      (currentSeconds >= startSeconds && currentSeconds < endSeconds) :
      (currentSeconds >= startSeconds || currentSeconds < endSeconds);

    if (!isActive) {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      return;
    }

    float progress = calculateProgress(currentSeconds, startSeconds, endSeconds);
    CRGB color = blend(warmColor, coldColor, (uint8_t)(progress * 255));
    FastLED.setBrightness(webBrightness);
    fill_solid(leds, NUM_LEDS, color);
}

void updateLEDs() {
  if (!powerOn) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    return;
  }

  bool manualMode = digitalRead(MODE_SWITCH_PIN) == HIGH;

  if (manualMode) {
    updateLEDsFromCircuit();
  } else {
    updateLEDsFromWeb();
  }
  FastLED.show();
}

// Web Server Handlers
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleSet() {
  if (server.hasArg("brightness")) {
    webBrightness = server.arg("brightness").toInt();
    server.send(200, "text/plain", "OK");
  }
}

void handleSetSchedule() {
  if (server.method() == HTTP_POST) {
    parseTime(server.arg("start"), &currentSchedule.startHour, &currentSchedule.startMinute);
    parseTime(server.arg("end"), &currentSchedule.endHour, &currentSchedule.endMinute);
    saveSchedule();
    server.send(200, "text/plain", "Schedule Updated");
  }
}

// Utility Functions
void parseTime(String timeStr, int* hour, int* minute) {
  int colonIndex = timeStr.indexOf(':');
  if (colonIndex != -1) {
    *hour = timeStr.substring(0, colonIndex).toInt();
    *minute = timeStr.substring(colonIndex+1).toInt();
  }
}

String formatTime(int hour, int minute) {
  return String(hour < 10 ? "0" : "") + hour + ":" + (minute < 10 ? "0" : "") + minute;
}

String getHTML() {
  return R"html(
    <!DOCTYPE html>
    <html>
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { background: #1a1a1a; color: white; font-family: Arial; padding: 20px; }
        .container { max-width: 400px; margin: auto; }
        .form-group { margin: 20px 0; }
        input[type=range] { width: 100%; margin: 10px 0; }
        input[type=time] { padding: 8px; margin: 5px 0; }
        button { background: #4CAF50; color: white; border: none; padding: 10px 20px; cursor: pointer; }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>LED Controller</h2>

        <div class="form-group">
          <label>Brightness:</label>
          <input type="range" id="brightness" min="0" max="255" value=")" + String(webBrightness) + R"html(">
        </div>

        <form action="/setSchedule" method="post">
          <div class="form-group">
            <label>Start Time:</label>
            <input type="time" name="start" value=")" + formatTime(currentSchedule.startHour, currentSchedule.startMinute) + R"html(">
          </div>
          <div class="form-group">
            <label>End Time:</label>
            <input type="time" name="end" value=")" + formatTime(currentSchedule.endHour, currentSchedule.endMinute) + R"html(">
          </div>
          <button type="submit">Save Schedule</button>
        </form>
      </div>

      <script>
        document.getElementById("brightness").oninput = function() {
          fetch(`/set?brightness=${this.value}`);
        };
      </script>
    </body>
    </html>
  )html";
}