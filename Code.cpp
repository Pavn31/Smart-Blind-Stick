#include <SoftwareSerial.h>
#include <NewSoftSerial.h> // or SoftwareSerial if you don't have NewSoftSerial
#include <TinyGPS++.h>
#include <BlynkSimpleStream.h> // assuming Blynk is used for IoT interface

// Pins for HC-SR04 Ultrasonic Sensor
const int trigPin = 9;
const int echoPin = 10;

// Water/Rain Sensor Pin
const int waterSensorPin = A0;

// Buzzer Pin
const int buzzerPin = 6;

// Small Display Screen (assuming using I2C OLED, use appropriate library accordingly)
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// GPS Module pins
static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
SoftwareSerial gpsSerial(RXPin, TXPin);

// GSM Module pins
SoftwareSerial gsmSerial(7, 8); // RX, TX for SIM800L or equivalent

// Variables
long duration;
int distance;

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(GPSBaud);
  gsmSerial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(waterSensorPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Starting...");
  display.display();

  // GSM module initialization - basic check
  gsmSerial.println("AT");
  delay(100);
}

void loop() {
  // Ultrasonic Sensor Reading
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  // Water Sensor Reading
  int waterValue = analogRead(waterSensorPin);

  // GPS Data Read
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Output to Display
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Distance: ");
  display.print(distance);
  display.println(" cm");
  
  display.print("Water: ");
  display.println(waterValue);

  if (gps.location.isValid()) {
    display.print("Lat: ");
    display.println(gps.location.lat(), 6);
    display.print("Lng: ");
    display.println(gps.location.lng(), 6);
  } else {
    display.println("No GPS Fix");
  }
  
  display.display();

  // GSM Module communication example: send SMS if water detected (example threshold)
  if (waterValue < 500) { // Adjust the threshold depending on your sensor reading
    sendSMS("Water detected!");
    digitalWrite(buzzerPin, HIGH);  // Turn buzzer ON
  } else {
    digitalWrite(buzzerPin, LOW);   // Turn buzzer OFF
  }

  delay(2000);
}

void sendSMS(String msg) {
  gsmSerial.println("AT+CMGF=1");  // Set SMS to text mode
  delay(100);
  gsmSerial.println("AT+CMGS=\"+1234567890\""); // Change to your phone number
  delay(100);
  gsmSerial.println(msg);
  delay(100);
  gsmSerial.write(26); // ASCII code of CTRL+Z to send SMS
  delay(5000);
}
