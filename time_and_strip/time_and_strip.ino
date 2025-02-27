#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <Preferences.h>
#include <time.h>

// Pins
#define LED_PIN         5
#define NUM_LEDS        60
#define BRIGHT_POT_PIN  34
#define HUE_POT_PIN     35
#define BUTTON_PIN      2

// WiFi Credentials
const char* ssid = "iPhone";
const char* password = "12345678";

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 0;

// Color Definitions
CRGB leds[NUM_LEDS];
CRGB warmColor = CRGB(255, 150, 100);
CRGB coldColor = CRGB(150, 180, 255);

// Web Server
WebServer server(80);
Preferences preferences;

// State Variables
volatile bool manualOverride = false;
int webBrightness = 128;
struct Schedule {
  int startHour;
  int startMinute;
  int endHour;
  int endMinute;
} currentSchedule;

// Debounce
volatile unsigned long lastDebounceTime = 0;

void IRAM_ATTR toggleOverride() {
  if (millis() - lastDebounceTime > 200) {
    manualOverride = !manualOverride;
    lastDebounceTime = millis();
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
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), toggleOverride, FALLING);

  // Load saved schedule
  loadSchedule();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
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
  delay(100);
}

// This saves the start and end times into non-volatile memory
// This lets them persist between reboots
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

void updateLEDs() {
  if (manualOverride) {
    // Manual mode - read from potentiometers
    int brightness = analogRead(BRIGHT_POT_PIN) >> 2;
    int hueValue = analogRead(HUE_POT_PIN) / 10.23; // 0-100

    CRGB color = blend(warmColor, coldColor, hueValue * 2.55);
    color.nscale8(brightness);
    fill_solid(leds, NUM_LEDS, color);
  } else {
    // Auto mode - time-based control
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int startMinutes = currentSchedule.startHour * 60 + currentSchedule.startMinute;
    int endMinutes = currentSchedule.endHour * 60 + currentSchedule.endMinute;

    bool isActive = (startMinutes < endMinutes) ?
      (currentMinutes >= startMinutes && currentMinutes <= endMinutes) :
      (currentMinutes >= startMinutes || currentMinutes <= endMinutes);

    if (isActive) {
      int totalDuration = abs(endMinutes - startMinutes);
      if (totalDuration == 0) totalDuration = 1; // Prevent division by zero

      int elapsed = (currentMinutes >= startMinutes) ?
        (currentMinutes - startMinutes) :
        ((1440 - startMinutes) + currentMinutes);

      float progress = constrain((float)elapsed / totalDuration, 0.0, 1.0);
      CRGB color = blend(warmColor, coldColor, (uint8_t)(progress * 255));
      color.nscale8(webBrightness);
      fill_solid(leds, NUM_LEDS, color);
    } else {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    }
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
