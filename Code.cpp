/*
 * ============================================================
 *  SMART BLIND STICK — Full Arduino Code
 *  Modules: Ultrasonic | Rain | Buzzer | GPS (NMEA) | GSM SMS
 * ============================================================
 *
 *  WIRING SUMMARY
 *  ─────────────────────────────────────────────────────────
 *  Ultrasonic HC-SR04 : Trig→D9   Echo→D10
 *  Rain Sensor        : AO→A0
 *  Buzzer (active)    : +→D6
 *  SOS Button         : one leg→D2, other leg→GND  (INPUT_PULLUP)
 *  GPS  (NEO-6M)      : TX→D4(Arduino RX)  RX→D3(Arduino TX)
 *  GSM  (SIM800L)     : TX→D7(Arduino RX)  RX→D8(Arduino TX)
 *
 *  IMPORTANT: SoftwareSerial can only LISTEN to one port at a
 *  time on AVR boards. This sketch switches .listen() between
 *  GPS and GSM as needed.
 * ============================================================
 */

#include <SoftwareSerial.h>

// ── Pin Definitions ──────────────────────────────────────────
#define TRIG_PIN    9
#define ECHO_PIN    10
#define RAIN_PIN    A0
#define BUZZER_PIN  6
#define BUTTON_PIN  2

// ── Emergency contact (replace with real number) ─────────────
#define SOS_NUMBER  "+91XXXXXXXXXX"

// ── Serial ports ─────────────────────────────────────────────
SoftwareSerial gpsSerial(4, 3);   // RX=D4, TX=D3
SoftwareSerial gsmSerial(7, 8);   // RX=D7, TX=D8

// ── GPS state ────────────────────────────────────────────────
String  gpsLatitude    = "N/A";
String  gpsLongitude   = "N/A";
String  gpsLatDir      = "";
String  gpsLonDir      = "";
bool    gpsFixed       = false;
uint8_t gpsFixQuality  = 0;
uint8_t gpsSatellites  = 0;

// ── Timing ───────────────────────────────────────────────────
unsigned long lastGpsPoll  = 0;
unsigned long lastSosSent  = 0;
const unsigned long GPS_INTERVAL  = 2000;   // read GPS every 2 s
const unsigned long SOS_COOLDOWN  = 15000;  // 15 s between SMS

// ============================================================
//  SETUP
// ============================================================
void setup() {
  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);

  Serial.println(F("=== Smart Blind Stick Started ==="));

  // Start listening on GPS by default
  gpsSerial.listen();

  // Initialise GSM module
  initGSM();
}

// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {

  // ── 1. Ultrasonic obstacle detection ───────────────────────
  int distance = measureDistance();
  Serial.print(F("Distance (cm): "));
  Serial.println(distance);

  if (distance > 0 && distance < 100) {
    // Beep frequency rises as obstacle gets closer
    int beepDelay = map(distance, 1, 100, 50, 500);
    tone(BUZZER_PIN, 1000);
    delay(beepDelay);
    noTone(BUZZER_PIN);
    delay(beepDelay);
  }

  // ── 2. Rain detection ──────────────────────────────────────
  int rainValue = analogRead(RAIN_PIN);
  Serial.print(F("Rain ADC: "));
  Serial.println(rainValue);

  if (rainValue < 500) {
    // Double-beep pattern for rain alert
    for (int i = 0; i < 2; i++) {
      tone(BUZZER_PIN, 1500);
      delay(150);
      noTone(BUZZER_PIN);
      delay(100);
    }
  }

  // ── 3. GPS polling ─────────────────────────────────────────
  if (millis() - lastGpsPoll >= GPS_INTERVAL) {
    lastGpsPoll = millis();
    readGPS();
  }

  // ── 4. SOS button ──────────────────────────────────────────
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastSosSent >= SOS_COOLDOWN) {
      lastSosSent = now;
      sendSOSSms();
    }
  }

  delay(50);
}

// ============================================================
//  ULTRASONIC — returns distance in cm (0 = error)
// ============================================================
int measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000); // 25 ms timeout → ~4 m max
  if (duration == 0) return 0;
  return (int)(duration * 0.034 / 2);
}

// ============================================================
//  GPS — READ & PARSE NMEA SENTENCES
// ============================================================

/*
 *  Reads all available bytes from the GPS module for up to
 *  800 ms and parses any $GPGGA or $GPRMC sentences found.
 *
 *  $GPGGA — Fix data (primary source for lat/lon)
 *  Field index (comma-separated after sentence ID):
 *   0  UTC time       hhmmss.ss
 *   1  Latitude       ddmm.mmmm
 *   2  N/S
 *   3  Longitude      dddmm.mmmm
 *   4  E/W
 *   5  Fix quality    0=no fix, 1=GPS, 2=DGPS
 *   6  Satellites
 *   7  HDOP
 *   8  Altitude
 *   ...
 */
void readGPS() {
  gpsSerial.listen();
  delay(10);  // let buffer settle after listen switch

  unsigned long start = millis();
  String sentence = "";

  while (millis() - start < 800) {
    if (gpsSerial.available()) {
      char c = gpsSerial.read();

      if (c == '$') {
        // Start of new sentence — read until newline
        sentence = gpsSerial.readStringUntil('\n');
        sentence.trim();

        if (sentence.startsWith("GPGGA") || sentence.startsWith("GNGGA")) {
          parseGPGGA(sentence);
        } else if (sentence.startsWith("GPRMC") || sentence.startsWith("GNRMC")) {
          parseGPRMC(sentence);   // fallback if GPGGA has no fix
        }
      }
    }
  }

  Serial.print(F("GPS Fix: "));
  Serial.print(gpsFixed ? "YES" : "NO");
  if (gpsFixed) {
    Serial.print(F("  Sats: ")); Serial.print(gpsSatellites);
    Serial.print(F("  Lat: "));  Serial.print(gpsLatitude);  Serial.print(gpsLatDir);
    Serial.print(F("  Lon: "));  Serial.print(gpsLongitude); Serial.println(gpsLonDir);
  } else {
    Serial.println(F("  (waiting for satellite fix)"));
  }
}

// ── Parse $GPGGA sentence ────────────────────────────────────
void parseGPGGA(const String &s) {
  // Split by commas
  String fields[15];
  int fieldCount = splitCSV(s, fields, 15);
  if (fieldCount < 7) return;

  // fields[0] = "GPGGA"
  // fields[1] = UTC time
  // fields[2] = raw latitude   e.g. "1234.5678"
  // fields[3] = N/S
  // fields[4] = raw longitude  e.g. "07712.3456"
  // fields[5] = E/W
  // fields[5] = fix quality
  // fields[6] = satellites

  gpsFixQuality = fields[5].toInt();

  if (gpsFixQuality == 0 || fields[2].length() == 0) {
    gpsFixed = false;
    return;
  }

  gpsFixed       = true;
  gpsSatellites  = fields[6].toInt();

  gpsLatitude  = convertNMEAtoDecimal(fields[2], true);
  gpsLatDir    = fields[3];
  gpsLongitude = convertNMEAtoDecimal(fields[4], false);
  gpsLonDir    = fields[5];
}

// ── Parse $GPRMC sentence (fallback) ─────────────────────────
void parseGPRMC(const String &s) {
  if (gpsFixed) return;   // GPGGA already gave us a fix

  String fields[12];
  int fieldCount = splitCSV(s, fields, 12);
  if (fieldCount < 7) return;

  // fields[2] = status A=active V=void
  if (fields[2] != "A") {
    gpsFixed = false;
    return;
  }

  gpsFixed     = true;
  gpsLatitude  = convertNMEAtoDecimal(fields[3], true);
  gpsLatDir    = fields[4];
  gpsLongitude = convertNMEAtoDecimal(fields[5], false);
  gpsLonDir    = fields[6];
}

/*
 *  convertNMEAtoDecimal()
 *  NMEA format: DDDMM.MMMMM  (degrees + decimal minutes)
 *  Decimal degrees = D + (MM.MMMMM / 60)
 *
 *  isLatitude: latitude has 2 degree digits, longitude has 3
 */
String convertNMEAtoDecimal(const String &raw, bool isLatitude) {
  if (raw.length() < 4) return "0.000000";

  int degreeDigits = isLatitude ? 2 : 3;
  double degrees   = raw.substring(0, degreeDigits).toDouble();
  double minutes   = raw.substring(degreeDigits).toDouble();
  double decimal   = degrees + (minutes / 60.0);

  // Format to 6 decimal places
  char buf[12];
  dtostrf(decimal, 9, 6, buf);
  return String(buf);
}

/*
 *  splitCSV()
 *  Splits a comma-separated String into an array.
 *  Returns the number of fields found.
 */
int splitCSV(const String &s, String *fields, int maxFields) {
  int idx = 0;
  int start = 0;
  for (int i = 0; i <= (int)s.length() && idx < maxFields; i++) {
    if (i == (int)s.length() || s[i] == ',') {
      fields[idx++] = s.substring(start, i);
      start = i + 1;
    }
  }
  return idx;
}

// ============================================================
//  GSM — INITIALISE MODULE
// ============================================================
void initGSM() {
  Serial.println(F("Initialising GSM..."));
  gsmSerial.listen();

  // Reset
  if (!sendATCommand("AT", "OK", 3000)) {
    Serial.println(F("[GSM] No response — check wiring/power"));
  }

  // Echo off
  sendATCommand("ATE0", "OK", 2000);

  // Check SIM
  if (sendATCommand("AT+CPIN?", "READY", 3000)) {
    Serial.println(F("[GSM] SIM OK"));
  } else {
    Serial.println(F("[GSM] SIM not ready — check card"));
  }

  // SMS text mode
  sendATCommand("AT+CMGF=1", "OK", 2000);

  // Set character set
  sendATCommand("AT+CSCS=\"GSM\"", "OK", 2000);

  // Check signal quality
  String sigResponse = sendATCommandRead("AT+CSQ", 2000);
  Serial.print(F("[GSM] Signal: "));
  Serial.println(sigResponse);

  // Switch back to GPS
  gpsSerial.listen();
  Serial.println(F("GSM init done."));
}

// ============================================================
//  GSM — SEND SOS SMS
// ============================================================
void sendSOSSms() {
  Serial.println(F("=== SOS TRIGGERED ==="));

  // Make sure we have the latest GPS position
  readGPS();

  gsmSerial.listen();
  delay(20);

  // ── Step 1: text mode ─────────────────────────────────────
  if (!sendATCommand("AT+CMGF=1", "OK", 3000)) {
    Serial.println(F("[GSM] Failed to set text mode"));
    gpsSerial.listen();
    return;
  }

  // ── Step 2: recipient number ──────────────────────────────
  String cmd = "AT+CMGS=\"";
  cmd += SOS_NUMBER;
  cmd += "\"";

  gsmSerial.println(cmd);
  delay(500);

  // Wait for '>' prompt
  if (!waitForResponse(">", 5000)) {
    Serial.println(F("[GSM] No '>' prompt received"));
    gsmSerial.write(27);  // ESC to cancel
    gpsSerial.listen();
    return;
  }

  // ── Step 3: compose message body ─────────────────────────
  gsmSerial.print(F("EMERGENCY! Blind stick SOS alert.\n"));

  if (gpsFixed) {
    gsmSerial.print(F("Location: "));
    gsmSerial.print(gpsLatitude);
    gsmSerial.print(gpsLatDir);
    gsmSerial.print(F(", "));
    gsmSerial.print(gpsLongitude);
    gsmSerial.print(gpsLonDir);
    gsmSerial.print(F("\nMaps: https://maps.google.com/?q="));
    gsmSerial.print(gpsLatitude);
    gsmSerial.print(F(","));
    gsmSerial.print(gpsLongitude);
  } else {
    gsmSerial.print(F("GPS fix not yet available. Please locate via last known position."));
  }

  // ── Step 4: send (Ctrl+Z = ASCII 26) ─────────────────────
  delay(200);
  gsmSerial.write(26);

  // ── Step 5: wait for +CMGS confirmation ──────────────────
  if (waitForResponse("+CMGS:", 10000)) {
    Serial.println(F("[GSM] SMS sent successfully!"));
    // Confirm beep: 3 short
    for (int i = 0; i < 3; i++) {
      tone(BUZZER_PIN, 2000); delay(100);
      noTone(BUZZER_PIN);     delay(80);
    }
  } else {
    Serial.println(F("[GSM] SMS send failed or timeout"));
    // Error beep: 1 long
    tone(BUZZER_PIN, 500); delay(800); noTone(BUZZER_PIN);
  }

  gpsSerial.listen();
}

// ============================================================
//  GSM HELPERS
// ============================================================

/*
 *  sendATCommand()
 *  Sends an AT command and returns true if the expected
 *  response string appears within the timeout window.
 */
bool sendATCommand(const char *cmd, const char *expected, unsigned long timeout) {
  // Flush incoming buffer
  while (gsmSerial.available()) gsmSerial.read();

  gsmSerial.println(cmd);
  Serial.print(F("[AT] >> "));
  Serial.println(cmd);

  return waitForResponse(expected, timeout);
}

/*
 *  waitForResponse()
 *  Accumulates GSM serial output until the expected token is
 *  found or the timeout expires.
 */
bool waitForResponse(const char *expected, unsigned long timeout) {
  String response = "";
  unsigned long start = millis();

  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      Serial.write(c);   // echo to Serial monitor for debug
    }
    if (response.indexOf(expected) != -1) return true;
    delay(10);
  }
  return false;
}

/*
 *  sendATCommandRead()
 *  Sends a command and returns the full response string.
 *  Useful for reading variable responses (e.g. AT+CSQ).
 */
String sendATCommandRead(const char *cmd, unsigned long timeout) {
  while (gsmSerial.available()) gsmSerial.read();
  gsmSerial.println(cmd);

  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (gsmSerial.available()) {
      response += (char)gsmSerial.read();
    }
    if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) break;
    delay(10);
  }
  response.trim();
  return response;
}
