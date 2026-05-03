#include <SoftwareSerial.h>

// Ultrasonic
#define trigPin 9
#define echoPin 10

// Rain sensor
#define rainPin A0

// Buzzer
#define buzzer 6

// Button
#define buttonPin 2

// GPS
SoftwareSerial gpsSerial(4, 3); // RX, TX

// GSM
SoftwareSerial gsmSerial(7, 8); // RX, TX

long duration;
int distance;
int rainValue;

String latitude = "0.0";
String longitude = "0.0";

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(9600);
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);

  Serial.println("Smart Blind Stick Started...");
}

void loop() {

  // ---------- ULTRASONIC ----------
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.println(distance);

  if (distance > 0 && distance < 50) {
    tone(buzzer, 1000);
    delay(distance * 5); // closer = faster beep
    noTone(buzzer);
  }

  // ---------- RAIN SENSOR ----------
  rainValue = analogRead(rainPin);
  Serial.print("Rain: ");
  Serial.println(rainValue);

  if (rainValue < 500) {  // adjust threshold
    tone(buzzer, 1500);
    delay(300);
    noTone(buzzer);
  }

  // ---------- GPS READ ----------
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    Serial.write(c);

    // basic parsing (simplified)
    if (c == '$') {
      String data = gpsSerial.readStringUntil('\n');

      if (data.startsWith("GPGGA")) {
        Serial.println(data);
      }
    }
  }

  // ---------- SOS BUTTON ----------
  if (digitalRead(buttonPin) == LOW) {
    sendSMS();
    delay(5000);
  }

  delay(100);
}

// ---------- SEND SMS ----------
void sendSMS() {
  Serial.println("Sending SMS...");

  gsmSerial.println("AT");
  delay(1000);

  gsmSerial.println("AT+CMGF=1");
  delay(1000);

  gsmSerial.println("AT+CMGS=\"+91XXXXXXXXXX\""); // replace number
  delay(1000);

  gsmSerial.print("HELP! I need assistance.\nLocation:\n");
  gsmSerial.print("Lat: ");
  gsmSerial.print(latitude);
  gsmSerial.print(" Lon: ");
  gsmSerial.print(longitude);

  delay(1000);
  gsmSerial.write(26); // CTRL+Z to send SMS
  delay(3000);

  Serial.println("SMS Sent!");
}