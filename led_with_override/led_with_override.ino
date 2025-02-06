const int brightPotPin = A0;  
const int huePotPin = A1;     
const int redLedPin = 9;      
const int greenLedPin = 10;   
const int buttonPin = 2;  

// Variables to store state
int totalBrightness = 0;
int hueRatio = 0;
bool manualOverride = false;
int manualBrightness = 128;
int manualHueRatio = 50;

void setup() {
  Serial.begin(9600);
  
  pinMode(brightPotPin, INPUT);
  pinMode(huePotPin, INPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(buttonPin), toggleOverride, FALLING);
}

void loop() {
  if (!manualOverride) {
    // Normal mode - read potentiometers
    totalBrightness = map(analogRead(brightPotPin), 0, 1023, 0, 255);
    hueRatio = map(analogRead(huePotPin), 0, 1023, 0, 100);
  } else {
    // Manual override mode
    totalBrightness = manualBrightness;
    hueRatio = manualHueRatio;
  }
  
  int redBrightness = map(hueRatio, 0, 100, 0, totalBrightness);
  int greenBrightness = totalBrightness - redBrightness;
  
  analogWrite(redLedPin, redBrightness);
  analogWrite(greenLedPin, greenBrightness);
  
  Serial.print("Override: ");
  Serial.print(manualOverride ? "ON" : "OFF");
  Serial.print(" | Total Brightness: ");
  Serial.print(totalBrightness);
  Serial.print(" | Hue Ratio: ");
  Serial.println(hueRatio);
  
  delay(100);
}

// Interrupt function to toggle override
void toggleOverride() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  
  if (interrupt_time - last_interrupt_time > 200) {
    manualOverride = !manualOverride;
    
    if (manualOverride) {
      manualBrightness = 128; // Default to 50% total brightness
      manualHueRatio = 50;    // Default to equal red/green ratio
    }
  }
  last_interrupt_time = interrupt_time;
}

// Function to set manual brightness and hue
void setManualSettings(int brightness, int hueRatio) {
  manualBrightness = constrain(brightness, 0, 255);
  manualHueRatio = constrain(hueRatio, 0, 100);
  
  if (manualOverride) {
    int redBrightness = map(manualHueRatio, 0, 100, 0, manualBrightness);
    int greenBrightness = manualBrightness - redBrightness;
    
    analogWrite(redLedPin, redBrightness);
    analogWrite(greenLedPin, greenBrightness);
  }
}