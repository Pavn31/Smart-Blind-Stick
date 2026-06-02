#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// HC-SR04
#define TRIG_PIN 9
#define ECHO_PIN 10

// Water Sensor
#define WATER_AO A0
#define WATER_DO 2

// Buzzer
#define BUZZER 6

// GPS
SoftwareSerial gpsSerial(4, 3);   // RX, TX

// GSM
SoftwareSerial gsmSerial(7, 8);   // RX, TX

TinyGPSPlus gps;

const char phoneNumber[] = "+91XXXXXXXXXX";

const int distanceThreshold = 20;
const int waterThreshold = 500;

unsigned long lastSMS = 0;
const unsigned long smsInterval = 60000;

void setup() {
  Serial.begin(9600);

  gpsSerial.begin(9600);
  gsmSerial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(WATER_DO, INPUT);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  delay(3000);

  gsmCommand("AT");
  gsmCommand("AT+CMGF=1");

  Serial.println("System Ready");
}

void loop() {

  // Read GPS continuously
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  float distance = getDistance();

  int waterAO = analogRead(WATER_AO);
  bool waterDO = digitalRead(WATER_DO) == LOW;

  bool objectDetected =
      (distance > 0 && distance < distanceThreshold);

  bool waterDetected =
      waterDO || (waterAO > waterThreshold);

  if (objectDetected || waterDetected) {

    digitalWrite(BUZZER, HIGH);

    if (millis() - lastSMS > smsInterval) {

      String msg = "ALERT!\n";

      if (objectDetected) {
        msg += "Object detected at ";
        msg += String(distance);
        msg += " cm\n";
      }

      if (waterDetected) {
        msg += "Water detected\n";
      }

      if (gps.location.isValid()) {
        msg += "https://maps.google.com/?q=";
        msg += String(gps.location.lat(), 6);
        msg += ",";
        msg += String(gps.location.lng(), 6);
      }
      else {
        msg += "GPS Not Fixed";
      }

      sendSMS(msg);

      lastSMS = millis();
    }
  }
  else {
    digitalWrite(BUZZER, LOW);
  }

  delay(500);
}

float getDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration =
      pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0)
    return -1;

  return duration * 0.034 / 2.0;
}

void sendSMS(String message) {

  gsmSerial.listen();

  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");

  delay(1000);

  gsmSerial.print(message);

  delay(500);

  gsmSerial.write(26);

  delay(5000);

  gpsSerial.listen();
}

void gsmCommand(String cmd) {

  gsmSerial.listen();

  gsmSerial.println(cmd);

  delay(1000);

  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());
  }

  gpsSerial.listen();
}
