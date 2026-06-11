/*
 * Smart Blind Stick — Arduino UNO
 * 
 * Pin Mapping:
 *   HC-SR04  TRIG → D9,  ECHO → D10
 *   Water    AO   → A0,  DO   → D2
 *   Buzzer        → D6
 *   GPS NEO-6M    RX → D4 (GPS TX), TX → D3 (GPS RX)  [SoftwareSerial]
 *   SIM800L GSM   RX → D0 (HW Serial TX), TX → D1 (HW Serial RX)
 *
 * NOTE: Disconnect SIM800L TX/RX from D0/D1 before uploading sketch,
 *       then reconnect after upload completes.
 */

#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// ─── Pin Definitions ─────────────────────────────────────────────────────────
#define TRIG_PIN        9
#define ECHO_PIN        10
#define WATER_AO        A0
#define WATER_DO        2
#define BUZZER_PIN      6

// ─── Thresholds ───────────────────────────────────────────────────────────────
const int   DIST_THRESHOLD  = 20;      // cm
const int   WATER_THRESHOLD = 500;     // analogRead value
const unsigned long SMS_INTERVAL = 60000UL; // ms between SMSes

// ─── Phone Number ─────────────────────────────────────────────────────────────
const char PHONE[] = "+91XXXXXXXXXX";  // ← replace with real number

// ─── GPS on SoftwareSerial (D4=RX, D3=TX) ────────────────────────────────────
SoftwareSerial gpsSerial(4, 3);
TinyGPSPlus    gps;

// ─── GSM on Hardware Serial (D0/D1) ──────────────────────────────────────────
// No SoftwareSerial for GSM — use Serial directly.
// During Serial.print() for debug we pause GSM reads briefly; this is fine
// because SIM800L buffers incoming data.

unsigned long lastSMS = 0;

// ─── Forward Declarations ────────────────────────────────────────────────────
float    getDistance();
void     sendSMS(const String& message);
bool     gsmCommand(const String& cmd, const String& expected = "OK",
                    unsigned long timeoutMs = 2000);
void     feedGPS(unsigned long ms);
String   buildAlertMessage(float dist, bool objDetected, bool waterDetected);

// ═══════════════════════════════════════════════════════════════════════════════
void setup() {
  // Hardware Serial shared by GSM + debug monitor.
  // Debug prints show in Serial Monitor while GSM is idle.
  Serial.begin(9600);

  gpsSerial.begin(9600);

  pinMode(TRIG_PIN,  OUTPUT);
  pinMode(ECHO_PIN,  INPUT);
  pinMode(WATER_DO,  INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // ── GSM Initialisation ────────────────────────────────────────────────────
  Serial.println(F("[SYS] Waiting for SIM800L boot..."));
  delay(5000);          // SIM800L needs ~3-5 s after power-on

  // Autobaud sync — send AT until we get OK
  bool gsmReady = false;
  for (uint8_t i = 0; i < 5; i++) {
    if (gsmCommand("AT", "OK", 2000)) { gsmReady = true; break; }
    delay(500);
  }
  if (!gsmReady) Serial.println(F("[GSM] WARNING: No response from SIM800L"));

  gsmCommand("ATE0",      "OK");        // echo off
  gsmCommand("AT+CMGF=1", "OK");        // SMS text mode

  // Wait for network registration (max ~30 s)
  Serial.println(F("[GSM] Waiting for network..."));
  for (uint8_t i = 0; i < 10; i++) {
    Serial.print(F("[GSM] AT+CREG? → "));
    Serial.flush();
    // Send and read raw response
    Serial.println("AT+CREG?");
    delay(1000);
    String resp = "";
    while (Serial.available()) {
      char c = Serial.read();
      resp += c;
    }
    Serial.print(resp);
    // Registered: +CREG: 0,1 or +CREG: 0,5 (roaming)
    if (resp.indexOf(",1") != -1 || resp.indexOf(",5") != -1) {
      Serial.println(F("[GSM] Network registered!"));
      break;
    }
    delay(2000);
  }

  Serial.println(F("[SYS] System ready."));
}

// ═══════════════════════════════════════════════════════════════════════════════
void loop() {
  // Feed GPS for up to 200 ms so we don't starve it between sensor reads
  feedGPS(200);

  float dist      = getDistance();
  int   waterAO   = analogRead(WATER_AO);
  bool  waterDO   = (digitalRead(WATER_DO) == LOW);

  bool objDetected   = (dist > 0 && dist < DIST_THRESHOLD);
  bool waterDetected = waterDO || (waterAO > WATER_THRESHOLD);

  // ── Debug output (visible in Serial Monitor when SIM800L is idle) ──────────
  Serial.print(F("[SENSOR] Dist: "));
  Serial.print(dist);
  Serial.print(F(" cm | WaterAO: "));
  Serial.print(waterAO);
  Serial.print(F(" | WaterDO: "));
  Serial.println(waterDO ? "WET" : "DRY");

  if (objDetected || waterDetected) {
    digitalWrite(BUZZER_PIN, HIGH);

    if ((millis() - lastSMS) > SMS_INTERVAL) {
      String msg = buildAlertMessage(dist, objDetected, waterDetected);
      sendSMS(msg);
      lastSMS = millis();
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Small delay; GPS is fed in next loop iteration
  delay(300);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Feed TinyGPS++ for a set number of milliseconds
// ═══════════════════════════════════════════════════════════════════════════════
void feedGPS(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// HC-SR04 distance measurement
// ═══════════════════════════════════════════════════════════════════════════════
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // 30 ms timeout (~5 m max)
  if (duration == 0) return -1.0;
  return duration * 0.034f / 2.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Build SMS payload
// ═══════════════════════════════════════════════════════════════════════════════
String buildAlertMessage(float dist, bool objDetected, bool waterDetected) {
  String msg = "ALERT!\n";

  if (objDetected) {
    msg += "Object at ";
    msg += String(dist, 1);
    msg += " cm\n";
  }
  if (waterDetected) {
    msg += "Water detected\n";
  }

  // Feed GPS a bit more before checking location
  feedGPS(500);

  if (gps.location.isValid() && gps.location.age() < 5000) {
    msg += "Location:\nhttps://maps.google.com/?q=";
    msg += String(gps.location.lat(), 6);
    msg += ",";
    msg += String(gps.location.lng(), 6);
  } else {
    msg += "GPS: No fix";
  }

  return msg;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Send SMS via SIM800L on hardware Serial
// ═══════════════════════════════════════════════════════════════════════════════
void sendSMS(const String& message) {
  Serial.print(F("AT+CMGS=\""));
  Serial.print(PHONE);
  Serial.println(F("\""));
  delay(1500);

  Serial.print(message);
  delay(500);
  Serial.write(26); // Ctrl+Z — send SMS
  delay(5000);

  Serial.println(F("[SMS] Sent."));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Send AT command and check for expected substring in response
// ═══════════════════════════════════════════════════════════════════════════════
bool gsmCommand(const String& cmd, const String& expected, unsigned long timeoutMs) {
  // Drain any leftover bytes
  while (Serial.available()) Serial.read();

  Serial.println(cmd);

  unsigned long start = millis();
  String resp = "";
  while (millis() - start < timeoutMs) {
    while (Serial.available()) {
      resp += (char)Serial.read();
    }
    if (resp.indexOf(expected) != -1) return true;
  }

  Serial.print(F("[GSM] No '"));
  Serial.print(expected);
  Serial.print(F("' for: "));
  Serial.println(cmd);
  return false;
}
