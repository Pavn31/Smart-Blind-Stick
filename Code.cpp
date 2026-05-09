/*
 * ============================================================
 *   SMART BLIND STICK - Arduino Nano
 * ============================================================
 *  Components:
 *    - HC-SR04  : Ultrasonic Distance Sensor (D9/D10)
 *    - Water Sensor : Rain / Water Detection (A0 / D2)
 *    - NEO-6M   : GPS Module (SoftwareSerial D4/D5)
 *    - SIM800L  : GSM Module  (SoftwareSerial D7/D8)
 *    - Buzzer   : Active/Passive Buzzer (D6)
 *    - OLED     : SSD1306 128x64 Display (I2C: A4=SDA, A5=SCL)
 *    - Blynk    : IoT Smartphone App (via GSM stream)
 *
 *  Libraries Required (install via Arduino IDE Library Manager):
 *    - TinyGPS++             (by Mikal Hart)
 *    - Adafruit SSD1306      (by Adafruit)
 *    - Adafruit GFX Library  (by Adafruit)
 *    - Blynk                 (by Volodymyr Shymanskyy)
 *
 *  Power: 9V or Li-ion Battery
 *  Board : Arduino Nano (ATmega328P, Old Bootloader)
 * ============================================================
 */

// ── BLYNK CONFIG (fill before compile) ────────────────────────
#define BLYNK_TEMPLATE_ID    "TMPLxxxxxxxx"      // From Blynk Dashboard
#define BLYNK_TEMPLATE_NAME  "SmartBlindStick"
#define BLYNK_AUTH_TOKEN     "YourBlynkAuthTokenHere"
#define BLYNK_PRINT          Serial

// ── LIBRARY INCLUDES ──────────────────────────────────────────
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// Uncomment the line below once Blynk token is configured:
// #include <BlynkSimpleStream.h>

// ═══════════════════════════════════════════════════════════════
//  PIN DEFINITIONS
// ═══════════════════════════════════════════════════════════════

// HC-SR04 Ultrasonic Sensor
const uint8_t TRIG_PIN      = 9;
const uint8_t ECHO_PIN      = 10;

// Water / Rain Sensor
const uint8_t WATER_AO_PIN  = A0;   // Analog output (0–1023)
const uint8_t WATER_DO_PIN  = 2;    // Digital output (LOW = wet)

// Buzzer
const uint8_t BUZZER_PIN    = 6;

// GPS NEO-6M  (SoftwareSerial)
const uint8_t GPS_RX        = 4;    // Arduino RX ← GPS TX
const uint8_t GPS_TX        = 5;    // Arduino TX → GPS RX

// GSM SIM800L (SoftwareSerial)
const uint8_t GSM_RX        = 7;    // Arduino RX ← GSM TX
const uint8_t GSM_TX        = 8;    // Arduino TX → GSM RX

// OLED Display (I2C)
const uint8_t SCREEN_W      = 128;
const uint8_t SCREEN_H      = 64;
const int8_t  OLED_RESET    = -1;   // Share Arduino reset
const uint8_t OLED_ADDR     = 0x3C;

// ═══════════════════════════════════════════════════════════════
//  USER CONFIGURATION
// ═══════════════════════════════════════════════════════════════

// ── Distance alert thresholds (centimetres) ───────────────────
const int DIST_DANGER   = 50;   // Very close  → rapid beep
const int DIST_WARNING  = 100;  // Medium close → medium beep
const int DIST_CAUTION  = 150;  // Far obstacle → slow beep

// ── Water sensor ──────────────────────────────────────────────
const int WATER_THRESHOLD = 500;  // Analog reading above = wet

// ── Emergency contact (international format) ──────────────────
const char EMERGENCY_NUM[] = "+91XXXXXXXXXX";   // ← Change this!

// ── Timing ────────────────────────────────────────────────────
const unsigned long SMS_COOLDOWN_MS    = 60000UL;  // Min gap between SMS (1 min)
const unsigned long DISPLAY_REFRESH_MS = 500UL;
const unsigned long DEBUG_PRINT_MS     = 1000UL;
const unsigned long GPS_READ_WINDOW_MS = 60UL;     // ms to feed GPS per loop

// Blynk virtual pins
#define VPIN_DISTANCE   V0
#define VPIN_WATER      V1
#define VPIN_GPS_LAT    V2
#define VPIN_GPS_LNG    V3
#define VPIN_ALERT      V4

// ═══════════════════════════════════════════════════════════════
//  OBJECT INSTANCES
// ═══════════════════════════════════════════════════════════════
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);
TinyGPSPlus    gps;
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

// ═══════════════════════════════════════════════════════════════
//  GLOBAL STATE
// ═══════════════════════════════════════════════════════════════
long     g_distance     = 400;   // cm
int      g_waterLevel   = 0;     // raw ADC
bool     g_waterDetect  = false;
double   g_lat          = 0.0;
double   g_lng          = 0.0;
bool     g_gpsValid     = false;
uint32_t g_gpsSats      = 0;

unsigned long g_lastSMS         = 0;
unsigned long g_lastDisplay     = 0;
unsigned long g_lastDebug       = 0;
unsigned long g_lastBuzz        = 0;

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(9600);

  // Pin config
  pinMode(TRIG_PIN,     OUTPUT);
  pinMode(ECHO_PIN,     INPUT);
  pinMode(BUZZER_PIN,   OUTPUT);
  pinMode(WATER_DO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  noTone(BUZZER_PIN);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[ERR] OLED not found. Check wiring."));
  }
  showSplash();

  // Serial ports
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);

  // GSM init
  initGSM();

  // Startup tone
  playStartupTone();

  Serial.println(F("\n[OK] Smart Blind Stick Ready\n"));
}

// ═══════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // 1 ── Ultrasonic reading
  g_distance = readUltrasonic();

  // 2 ── Water / Rain sensor
  g_waterLevel  = analogRead(WATER_AO_PIN);
  bool doPinWet = (digitalRead(WATER_DO_PIN) == LOW);
  g_waterDetect = doPinWet || (g_waterLevel > WATER_THRESHOLD);

  // 3 ── Feed GPS parser
  readGPS();

  // 4 ── Buzzer logic
  handleBuzzer(now);

  // 5 ── OLED update
  if (now - g_lastDisplay >= DISPLAY_REFRESH_MS) {
    updateDisplay();
    g_lastDisplay = now;
  }

  // 6 ── SMS alert if water detected (with cooldown)
  if (g_waterDetect && (now - g_lastSMS >= SMS_COOLDOWN_MS)) {
    sendSMSAlert();
    g_lastSMS = now;
  }

  // 7 ── Blynk push (uncomment if using Blynk)
  // Blynk.run();
  // pushBlynk();

  // 8 ── Serial debug
  if (now - g_lastDebug >= DEBUG_PRINT_MS) {
    debugPrint();
    g_lastDebug = now;
  }
}

// ═══════════════════════════════════════════════════════════════
//  HC-SR04 ULTRASONIC SENSOR
// ═══════════════════════════════════════════════════════════════
/**
 * Sends a 10 µs trigger pulse and measures echo duration.
 * Returns distance in cm (clamped 2–400).
 * pulseIn timeout = 30 ms → max measurable ~510 cm (safe for 400 cm sensor).
 */
long readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);  // µs
  if (duration == 0) return 400;                      // No echo → max range

  long dist = duration * 17L / 1000L;                // cm = (µs × 0.034) / 2
  return constrain(dist, 2, 400);
}

// ═══════════════════════════════════════════════════════════════
//  BUZZER
// ═══════════════════════════════════════════════════════════════
/**
 * Three-tier distance alert + separate water alert pattern.
 *   Water detected  → triple high beep (priority override)
 *   dist ≤ 50 cm    → rapid 100 ms beeps @ 2000 Hz (DANGER)
 *   dist ≤ 100 cm   → medium 300 ms beeps @ 1500 Hz (WARNING)
 *   dist ≤ 150 cm   → slow  700 ms beeps @ 1000 Hz (CAUTION)
 *   dist > 150 cm   → silence (safe)
 */
void handleBuzzer(unsigned long now) {
  if (g_waterDetect) {
    tripleBeep(2500, 200, 300);
    return;
  }

  if (g_distance <= DIST_DANGER) {
    if (now - g_lastBuzz >= 100) {
      tone(BUZZER_PIN, 2000, 80);
      g_lastBuzz = now;
    }
  } else if (g_distance <= DIST_WARNING) {
    if (now - g_lastBuzz >= 300) {
      tone(BUZZER_PIN, 1500, 120);
      g_lastBuzz = now;
    }
  } else if (g_distance <= DIST_CAUTION) {
    if (now - g_lastBuzz >= 700) {
      tone(BUZZER_PIN, 1000, 150);
      g_lastBuzz = now;
    }
  } else {
    noTone(BUZZER_PIN);
  }
}

/** Beep N times with given frequency, on-time, and gap (ms). */
void tripleBeep(int freq, int onMs, int gapMs) {
  for (uint8_t i = 0; i < 3; i++) {
    tone(BUZZER_PIN, freq, onMs);
    delay(onMs + gapMs);
  }
}

void playStartupTone() {
  const int notes[] = {1000, 1500, 2000, 2500};
  for (uint8_t i = 0; i < 4; i++) {
    tone(BUZZER_PIN, notes[i], 150);
    delay(200);
  }
  noTone(BUZZER_PIN);
}

// ═══════════════════════════════════════════════════════════════
//  GPS NEO-6M
// ═══════════════════════════════════════════════════════════════
/**
 * Feeds available GPS bytes into TinyGPS++ for up to GPS_READ_WINDOW_MS ms.
 * Updates globals g_lat, g_lng, g_gpsValid, g_gpsSats.
 */
void readGPS() {
  unsigned long start = millis();
  while (millis() - start < GPS_READ_WINDOW_MS) {
    if (gpsSerial.available()) {
      char c = gpsSerial.read();
      if (gps.encode(c)) {
        if (gps.location.isValid() && gps.location.age() < 2000) {
          g_lat      = gps.location.lat();
          g_lng      = gps.location.lng();
          g_gpsValid = true;
        }
        g_gpsSats = gps.satellites.isValid() ? gps.satellites.value() : 0;
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════════
//  GSM SIM800L
// ═══════════════════════════════════════════════════════════════
/**
 * Sends AT commands to configure SIM800L for SMS text mode.
 * Waits for each response with a short timeout.
 */
void initGSM() {
  Serial.println(F("[GSM] Initializing..."));
  display.clearDisplay();
  display.setCursor(0, 20);
  display.setTextSize(1);
  display.println(F("  GSM Initializing..."));
  display.display();

  sendATCommand("AT",           2000, "OK");    // Basic handshake
  sendATCommand("ATE0",         1000, "OK");    // Echo off
  sendATCommand("AT+CMGF=1",    1000, "OK");    // SMS text mode
  sendATCommand("AT+CNMI=1,2,0,0,0", 1000, "OK"); // Route SMS to serial
  sendATCommand("AT+CSCS=\"GSM\"", 1000, "OK"); // GSM charset

  Serial.println(F("[GSM] Ready"));
}

/**
 * Sends an AT command, waits up to timeoutMs, prints response.
 * Returns true if expected keyword found.
 */
bool sendATCommand(const char* cmd, unsigned long timeoutMs, const char* expected) {
  gsmSerial.println(cmd);
  unsigned long t = millis();
  String resp = "";
  while (millis() - t < timeoutMs) {
    while (gsmSerial.available()) {
      resp += (char)gsmSerial.read();
    }
    if (resp.indexOf(expected) != -1) {
      Serial.print(F("[GSM] ")); Serial.print(cmd);
      Serial.print(F(" → ")); Serial.println(expected);
      return true;
    }
  }
  Serial.print(F("[GSM] TIMEOUT: ")); Serial.println(cmd);
  return false;
}

/**
 * Builds and sends an SMS to EMERGENCY_NUM.
 * Includes GPS Google Maps link if fix available.
 */
void sendSMSAlert() {
  Serial.println(F("[GSM] Sending SMS alert..."));

  String msg = "ALERT! Smart Blind Stick\n";
  msg += "Water/Rain detected!\n";

  if (g_gpsValid) {
    msg += "Location:\n";
    msg += "https://maps.google.com/?q=";
    msg += String(g_lat, 6);
    msg += ",";
    msg += String(g_lng, 6);
  } else {
    msg += "GPS: Searching for fix...";
  }

  gsmSerial.println("AT+CMGF=1");
  delay(300);
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(EMERGENCY_NUM);
  gsmSerial.println("\"");
  delay(300);
  gsmSerial.print(msg);
  delay(100);
  gsmSerial.write(26);   // ASCII SUB / Ctrl+Z → sends SMS
  delay(5000);           // Wait for modem response

  Serial.println(F("[GSM] SMS sent!"));

  // Show confirmation on display
  display.clearDisplay();
  display.setCursor(10, 20);
  display.setTextSize(1);
  display.println(F("  SMS Alert Sent!"));
  display.display();
  delay(1500);
}

// ═══════════════════════════════════════════════════════════════
//  BLYNK IoT  (Uncomment & configure to enable)
// ═══════════════════════════════════════════════════════════════
/*
void pushBlynk() {
  static unsigned long lastBlynk = 0;
  if (millis() - lastBlynk < 2000) return;   // 2-second push interval
  lastBlynk = millis();

  Blynk.virtualWrite(VPIN_DISTANCE, g_distance);
  Blynk.virtualWrite(VPIN_WATER,    g_waterDetect ? 1 : 0);

  if (g_gpsValid) {
    Blynk.virtualWrite(VPIN_GPS_LAT, g_lat);
    Blynk.virtualWrite(VPIN_GPS_LNG, g_lng);
  }

  if (g_distance <= DIST_DANGER || g_waterDetect) {
    Blynk.virtualWrite(VPIN_ALERT, "⚠ HAZARD DETECTED");
    Blynk.logEvent("hazard_alert", "Obstacle or water detected!");
  }
}
*/

// ═══════════════════════════════════════════════════════════════
//  OLED SSD1306 DISPLAY
// ═══════════════════════════════════════════════════════════════
/** Splash screen shown on boot. */
void showSplash() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(12, 8);
  display.println(F("SMART BLIND STICK"));
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setCursor(20, 28);
  display.setTextSize(1);
  display.println(F("Initializing..."));
  display.setCursor(8, 46);
  display.println(F("All modules loading"));
  display.display();
  delay(2500);
}

/**
 * Main display layout:
 *   Row 0  : Title bar
 *   Row 12 : Distance (cm) + alert level badge
 *   Row 24 : Water status
 *   Row 34 : GPS coordinates or search status
 *   Row 54 : Status bar (GPS sats / GSM)
 */
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // ── Title ──
  display.setTextSize(1);
  display.setCursor(14, 0);
  display.print(F("SMART BLIND STICK"));
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // ── Distance ──
  display.setCursor(0, 12);
  display.print(F("Dist:"));
  display.setTextSize(1);

  if (g_distance < 1000) {
    display.setCursor(33, 11);
    display.setTextSize(2);
    display.print(g_distance);
    display.setTextSize(1);
    display.print(F("cm"));
  }

  // Alert badge (right-aligned)
  display.setCursor(95, 12);
  if      (g_distance <= DIST_DANGER)  display.print(F("[!!!]"));
  else if (g_distance <= DIST_WARNING) display.print(F("[ !! ]"));
  else if (g_distance <= DIST_CAUTION) display.print(F("[ !  ]"));
  else                                 display.print(F("[ OK ]"));

  display.drawLine(0, 29, 127, 29, SSD1306_WHITE);

  // ── Water status ──
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print(F("Water: "));
  if (g_waterDetect) {
    display.print(F("** DETECTED **"));
  } else {
    display.print(F("Clear ("));
    display.print(g_waterLevel);
    display.print(F(")"));
  }

  display.drawLine(0, 42, 127, 42, SSD1306_WHITE);

  // ── GPS ──
  display.setCursor(0, 45);
  if (g_gpsValid) {
    display.print(F("Lat:"));  display.println(g_lat, 4);
    display.setCursor(0, 54);
    display.print(F("Lng:"));  display.print(g_lng, 4);
  } else {
    display.print(F("GPS: Searching"));
    display.setCursor(0, 54);
    display.print(F("Sats: "));
    display.print(g_gpsSats);
  }

  display.display();
}

// ═══════════════════════════════════════════════════════════════
//  DEBUG (Serial Monitor)
// ═══════════════════════════════════════════════════════════════
void debugPrint() {
  Serial.print(F("[SBS] Dist="));
  Serial.print(g_distance);
  Serial.print(F("cm | Water="));
  Serial.print(g_waterDetect ? F("YES") : F("no"));
  Serial.print(F(" (ADC="));
  Serial.print(g_waterLevel);
  Serial.print(F(") | GPS="));
  if (g_gpsValid) {
    Serial.print(g_lat, 5);
    Serial.print(F(","));
    Serial.print(g_lng, 5);
    Serial.print(F(" Sats="));
    Serial.print(g_gpsSats);
  } else {
    Serial.print(F("Searching (sats="));
    Serial.print(g_gpsSats);
    Serial.print(F(")"));
  }
  Serial.println();
}

// ═══════════════════════════════════════════════════════════════
//  END OF FILE
// ═══════════════════════════════════════════════════════════════
