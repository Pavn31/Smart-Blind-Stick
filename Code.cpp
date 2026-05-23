/*
 * Smart Monitoring System
 * Components: HC-SR04 Ultrasonic, Water/Rain Sensor, GPS Neo-6M, GSM SIM800L, Buzzer
 * Board: Arduino Nano
 *
 * Pin Mapping:
 *   HC-SR04  : TRIG=D9,  ECHO=D10
 *   Water    : AO=A0,    DO=D2
 *   GPS      : TX=D4,    RX=D3   (SoftwareSerial)
 *   GSM      : TX=D7,    RX=D8   (SoftwareSerial)
 *   Buzzer   : D6
 */

#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// ─── Pin Definitions ────────────────────────────────────────────────
#define TRIG_PIN        9
#define ECHO_PIN        10
#define WATER_AO_PIN    A0
#define WATER_DO_PIN    2
#define BUZZER_PIN      6

// ─── Serial Ports ───────────────────────────────────────────────────
// GPS: Arduino listens on D4 (GPS TX), sends on D3 (GPS RX)
SoftwareSerial gpsSerial(4, 3);      // RX=D4, TX=D3

// GSM: Arduino listens on D7 (GSM TX), sends on D8 (GSM RX)
SoftwareSerial gsmSerial(7, 8);      // RX=D7, TX=D8

TinyGPSPlus gps;

// ─── Configuration ──────────────────────────────────────────────────
const char*   PHONE_NUMBER        = "+91XXXXXXXXXX"; // <-- Your number
const int     DISTANCE_THRESHOLD  = 20;   // cm  — object too close
const int     WATER_AO_THRESHOLD  = 500;  // 0-1023, above = wet
const unsigned long ALERT_INTERVAL = 60000UL; // min gap between SMS (ms)

// ─── State ──────────────────────────────────────────────────────────
unsigned long lastAlertTime       = 0;
bool          gsmReady            = false;

// ════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);

  pinMode(TRIG_PIN,     OUTPUT);
  pinMode(ECHO_PIN,     INPUT);
  pinMode(WATER_DO_PIN, INPUT);
  pinMode(BUZZER_PIN,   OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  Serial.println(F("=== Smart Monitoring System Starting ==="));

  initGSM();
}

// ════════════════════════════════════════════════════════════════════
void loop() {
  // 1. Feed GPS data
  while (gpsSerial.available())
    gps.encode(gpsSerial.read());

  // 2. Read sensors
  float    distance   = readUltrasonic();
  int      waterAO    = analogRead(WATER_AO_PIN);
  bool     waterDO    = digitalRead(WATER_DO_PIN) == LOW; // LOW = wet
  bool     objectNear = (distance > 0 && distance < DISTANCE_THRESHOLD);
  bool     isWet      = waterDO || (waterAO > WATER_AO_THRESHOLD);

  // 3. Debug output
  Serial.print(F("Dist: ")); Serial.print(distance);  Serial.print(F(" cm | "));
  Serial.print(F("WaterAO: ")); Serial.print(waterAO); Serial.print(F(" | "));
  Serial.print(F("WaterDO: ")); Serial.print(waterDO ? "WET" : "DRY"); Serial.print(F(" | "));
  Serial.print(F("GPS fix: ")); Serial.println(gps.location.isValid() ? "YES" : "NO");

  // 4. Alert logic
  bool alertNeeded = objectNear || isWet;
  unsigned long now = millis();

  if (alertNeeded) {
    activateBuzzer(true);

    // Send SMS at most once per ALERT_INTERVAL
    if (gsmReady && (now - lastAlertTime >= ALERT_INTERVAL)) {
      String message = buildAlertMessage(objectNear, isWet, distance, waterAO);
      sendSMS(PHONE_NUMBER, message);
      lastAlertTime = now;
    }
  } else {
    activateBuzzer(false);
  }

  delay(500);
}

// ─── HC-SR04 ────────────────────────────────────────────────────────
float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // 30ms timeout
  if (duration == 0) return -1.0;                   // no echo
  return duration * 0.034 / 2.0;
}

// ─── Buzzer ─────────────────────────────────────────────────────────
void activateBuzzer(bool state) {
  digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
}

// ─── GSM Init ───────────────────────────────────────────────────────
void initGSM() {
  Serial.println(F("Initializing GSM..."));
  delay(3000); // wait for SIM800L to boot

  sendAT("AT", 2000);            // basic check
  sendAT("AT+CMGF=1", 1000);     // text mode SMS
  sendAT("AT+CSCS=\"GSM\"", 1000);

  gsmReady = true;
  Serial.println(F("GSM Ready."));
}

// ─── Send AT Command ────────────────────────────────────────────────
String sendAT(const char* cmd, unsigned int timeout) {
  gsmSerial.println(cmd);
  unsigned long t = millis();
  String resp = "";
  while (millis() - t < timeout) {
    while (gsmSerial.available())
      resp += (char)gsmSerial.read();
  }
  Serial.print(F("[GSM] ")); Serial.print(cmd);
  Serial.print(F(" -> ")); Serial.println(resp);
  return resp;
}

// ─── Send SMS ───────────────────────────────────────────────────────
void sendSMS(const char* number, String message) {
  Serial.print(F("Sending SMS to ")); Serial.println(number);

  gsmSerial.print(F("AT+CMGS=\""));
  gsmSerial.print(number);
  gsmSerial.println(F("\""));
  delay(1000);

  gsmSerial.print(message);
  delay(500);

  gsmSerial.write(26); // Ctrl+Z  — send
  delay(3000);
  Serial.println(F("SMS sent."));
}

// ─── Build Alert Message ────────────────────────────────────────────
String buildAlertMessage(bool objectNear, bool isWet,
                         float distance, int waterAO) {
  String msg = "ALERT!\n";

  if (objectNear) {
    msg += "Object detected at ";
    msg += String(distance, 1);
    msg += " cm\n";
  }
  if (isWet) {
    msg += "Water/Rain detected (AO=";
    msg += String(waterAO);
    msg += ")\n";
  }

  // Append GPS if available
  if (gps.location.isValid()) {
    msg += "Location:\n";
    msg += "Lat: ";  msg += String(gps.location.lat(), 6); msg += "\n";
    msg += "Lng: ";  msg += String(gps.location.lng(), 6); msg += "\n";
    msg += "https://maps.google.com/?q=";
    msg += String(gps.location.lat(), 6);
    msg += ",";
    msg += String(gps.location.lng(), 6);
  } else {
    msg += "GPS: No fix yet";
  }

  return msg;
}