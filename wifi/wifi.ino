#include <WiFi.h>
//Select FireBeetle-ESP32 for board
// Replace with your network credentials
const char* ssid = "iPhone";
const char* password = "12345678";

const int ledPin = 13;

// Create a web server on port 80
WiFiServer server(80);

void setup() {
  // Initialize the serial monitor
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("Connected to Wi-Fi");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.print("Use this URL to connect: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void loop() {
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("New client connected");
  while (!client.available()) {
    delay(1);
  }

  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  // Match the request
  if (request.indexOf("/LED=ON") != -1) {
    digitalWrite(ledPin, HIGH);
    Serial.println("LED is ON");
  } else if (request.indexOf("/LED=OFF") != -1) {
    digitalWrite(ledPin, LOW);
    Serial.println("LED is OFF");
  }

  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("");
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<h1>ESP32 LED Control</h1>");
  client.println("<p><a href=\"/LED=ON\"><button>Turn ON</button></a></p>");
  client.println("<p><a href=\"/LED=OFF\"><button>Turn OFF</button></a></p>");
  client.println("</html>");

  delay(1);
  Serial.println("Client disconnected");
}