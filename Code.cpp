/*
 * =============================================
 *        SMART BLIND STICK - C++ Version
 * =============================================
 * Components:
 *   - Arduino Nano (ATmega328P)
 *   - HC-SR04 Ultrasonic Sensor
 *   - Water/Rain Sensor Module
 *   - GPS Module (NEO-6M)
 *   - GSM Module (SIM800L)
 *   - Buzzer (Active)
 *   - 16x2 I2C LCD Display
 *   - 9V / Li-ion Battery
 *
 * Libraries Required:
 *   - TinyGPS++ by Mikal Hart
 *   - LiquidCrystal_I2C by Frank de Brabander
 * =============================================
 */

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =============================================
//  CONFIGURATION
// =============================================
namespace Config {
    constexpr const char* CAREGIVER_NUMBER = "+91XXXXXXXXXX"; // Change this!

    // Distance thresholds (cm)
    constexpr uint8_t  DIST_CRITICAL = 30;
    constexpr uint8_t  DIST_WARNING  = 70;
    constexpr uint8_t  DIST_CAUTION  = 120;

    // Water sensor threshold (0–1023, lower = wetter)
    constexpr uint16_t WATER_THRESHOLD = 500;

    // Timing (ms)
    constexpr uint16_t LCD_UPDATE_INTERVAL  = 600;
    constexpr uint32_t AUTO_SMS_INTERVAL    = 120000UL; // 2 minutes
    constexpr uint16_t GPS_READ_WINDOW      = 50;
    constexpr uint16_t DEBOUNCE_DELAY       = 50;
}

// =============================================
//  PIN DEFINITIONS
// =============================================
namespace Pins {
    constexpr uint8_t TRIG         = 9;
    constexpr uint8_t ECHO         = 10;
    constexpr uint8_t WATER_AO     = A0;
    constexpr uint8_t WATER_DO     = 7;
    constexpr uint8_t BUZZER       = 6;
    constexpr uint8_t PANIC_BUTTON = 8;   // Active LOW
    constexpr uint8_t GSM_RX       = 12;
    constexpr uint8_t GSM_TX       = 11;
    constexpr uint8_t GPS_RX       = 4;
    constexpr uint8_t GPS_TX       = 3;
}

// =============================================
//  ENUMERATIONS
// =============================================
enum class ObstacleLevel : uint8_t {
    SAFE,
    CAUTION,
    WARNING,
    DANGER
};

enum class WaterStatus : uint8_t {
    DRY,
    DAMP,
    WET
};

enum class AlertReason : uint8_t {
    PANIC,
    WATER
};

// =============================================
//  GPS MANAGER CLASS
// =============================================
class GPSManager {
public:
    GPSManager(uint8_t rxPin, uint8_t txPin)
        : serial_(rxPin, txPin), fixed_(false), lat_(0.0), lng_(0.0) {}

    void begin() {
        serial_.begin(9600);
    }

    void update() {
        serial_.listen();
        unsigned long start = millis();
        while (millis() - start < Config::GPS_READ_WINDOW) {
            if (serial_.available()) {
                gps_.encode(serial_.read());
            }
        }
        if (gps_.location.isValid() && gps_.location.isUpdated()) {
            lat_   = gps_.location.lat();
            lng_   = gps_.location.lng();
            fixed_ = true;
        }
    }

    bool    isFixed()    const { return fixed_; }
    double  getLat()     const { return lat_;   }
    double  getLng()     const { return lng_;   }
    uint8_t getSatCount() const { return gps_.satellites.isValid() ? gps_.satellites.value() : 0; }

    SoftwareSerial& getSerial() { return serial_; }

private:
    SoftwareSerial serial_;
    TinyGPSPlus    gps_;
    bool           fixed_;
    double         lat_;
    double         lng_;
};

// =============================================
//  GSM MANAGER CLASS
// =============================================
class GSMManager {
public:
    GSMManager(uint8_t rxPin, uint8_t txPin)
        : serial_(rxPin, txPin) {}

    void begin() {
        serial_.begin(9600);
        delay(1000);
        init_();
    }

    void listen() {
        serial_.listen();
    }

    bool sendSMS(const String& number, const String& message) {
        serial_.listen();

        serial_.print(F("AT+CMGS=\""));
        serial_.print(number);
        serial_.println(F("\""));
        delay(1000);

        serial_.print(message);
        delay(500);
        serial_.write(26); // Ctrl+Z
        delay(3000);

        Serial.println(F("SMS Sent!"));
        return true;
    }

    void sendAT(const String& cmd, uint16_t timeout = 1000) {
        serial_.listen();
        serial_.println(cmd);
        unsigned long t = millis();
        while (millis() - t < timeout) {
            if (serial_.available()) {
                Serial.write(serial_.read());
            }
        }
    }

private:
    SoftwareSerial serial_;

    void init_() {
        Serial.println(F("Initializing GSM..."));
        sendAT("AT",           1000);
        sendAT("AT+CMGF=1",   1000);
        sendAT("AT+CSCS=\"GSM\"", 1000);
        Serial.println(F("GSM Ready."));
    }
};

// =============================================
//  ULTRASONIC SENSOR CLASS
// =============================================
class UltrasonicSensor {
public:
    UltrasonicSensor(uint8_t trigPin, uint8_t echoPin)
        : trigPin_(trigPin), echoPin_(echoPin), distance_(400) {}

    void begin() {
        pinMode(trigPin_, OUTPUT);
        pinMode(echoPin_, INPUT);
    }

    uint16_t measure() {
        digitalWrite(trigPin_, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin_, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin_, LOW);

        long dur = pulseIn(echoPin_, HIGH, 25000UL);
        if (dur == 0) {
            distance_ = 400; // No echo
        } else {
            distance_ = constrain((uint16_t)(dur * 0.034 / 2), 0, 400);
        }
        return distance_;
    }

    uint16_t getDistance() const { return distance_; }

    ObstacleLevel getLevel() const {
        if (distance_ <= Config::DIST_CRITICAL) return ObstacleLevel::DANGER;
        if (distance_ <= Config::DIST_WARNING)  return ObstacleLevel::WARNING;
        if (distance_ <= Config::DIST_CAUTION)  return ObstacleLevel::CAUTION;
        return ObstacleLevel::SAFE;
    }

    const char* getLevelString() const {
        switch (getLevel()) {
            case ObstacleLevel::DANGER:  return "DANGER! ";
            case ObstacleLevel::WARNING: return "WARNING ";
            case ObstacleLevel::CAUTION: return "CAUTION ";
            default:                     return "SAFE    ";
        }
    }

private:
    uint8_t  trigPin_;
    uint8_t  echoPin_;
    uint16_t distance_;
};

// =============================================
//  WATER SENSOR CLASS
// =============================================
class WaterSensor {
public:
    WaterSensor(uint8_t analogPin, uint8_t digitalPin)
        : aPin_(analogPin), dPin_(digitalPin), value_(1023) {}

    void begin() {
        pinMode(dPin_, INPUT);
    }

    void read() {
        value_   = analogRead(aPin_);
        dValue_  = digitalRead(dPin_);
    }

    bool isWetDetected() const {
        return (value_ < Config::WATER_THRESHOLD) || (dValue_ == LOW);
    }

    WaterStatus getStatus() const {
        if (value_ < 300)                    return WaterStatus::WET;
        if (value_ < Config::WATER_THRESHOLD) return WaterStatus::DAMP;
        return WaterStatus::DRY;
    }

    const char* getStatusString() const {
        switch (getStatus()) {
            case WaterStatus::WET:  return "WET / RAIN!!";
            case WaterStatus::DAMP: return "DAMP        ";
            default:                return "DRY - OK    ";
        }
    }

    uint16_t getValue() const { return value_; }

private:
    uint8_t  aPin_;
    uint8_t  dPin_;
    uint16_t value_;
    bool     dValue_ = false;
};

// =============================================
//  BUZZER CLASS
// =============================================
class Buzzer {
public:
    explicit Buzzer(uint8_t pin) : pin_(pin) {}

    void begin() {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, LOW);
    }

    void beep(uint16_t onMs, uint16_t offMs = 0) {
        digitalWrite(pin_, HIGH);
        delay(onMs);
        digitalWrite(pin_, LOW);
        if (offMs > 0) delay(offMs);
    }

    void tripleBeep() {
        for (uint8_t i = 0; i < 3; i++) beep(80, 60);
    }

    void panicBeep() {
        for (uint8_t i = 0; i < 5; i++) beep(300, 150);
    }

    void confirmBeep() {
        for (uint8_t i = 0; i < 2; i++) beep(200, 200);
    }

    void startupBeep() {
        beep(100, 100);
        beep(100, 100);
        beep(200, 0);
    }

    void off() {
        digitalWrite(pin_, LOW);
    }

private:
    uint8_t pin_;
};

// =============================================
//  DISPLAY MANAGER CLASS
// =============================================
class DisplayManager {
public:
    DisplayManager() : lcd_(0x27, 16, 2), page_(0) {}

    void begin() {
        lcd_.init();
        lcd_.backlight();
        showSplash_();
    }

    void showObstaclePage(uint16_t distance, const char* levelStr) {
        lcd_.clear();
        lcd_.setCursor(0, 0);
        lcd_.print(F("Obstacle: "));
        if (distance >= 400) {
            lcd_.print(F("CLEAR"));
        } else {
            lcd_.print(distance);
            lcd_.print(F("cm"));
        }
        lcd_.setCursor(0, 1);
        lcd_.print(F("Stat: "));
        lcd_.print(levelStr);
    }

    void showWaterPage(const char* statusStr, uint16_t rawValue) {
        lcd_.clear();
        lcd_.setCursor(0, 0);
        lcd_.print(F("Water Sensor:   "));
        lcd_.setCursor(0, 1);
        lcd_.print(statusStr);
    }

    void showGPSPage(bool fixed, double lat, double lng, uint8_t sats) {
        lcd_.clear();
        lcd_.setCursor(0, 0);
        if (fixed) {
            lcd_.print(F("GPS: FIXED "));
            lcd_.print(sats);
            lcd_.print(F("sat"));
            lcd_.setCursor(0, 1);
            lcd_.print(lat, 4);
            lcd_.print(F(","));
            lcd_.print(lng, 4);
        } else {
            lcd_.print(F("GPS: Searching.."));
            lcd_.setCursor(0, 1);
            lcd_.print(F("No Fix Yet      "));
        }
    }

    void nextPage() {
        page_ = (page_ + 1) % 3;
    }

    uint8_t getPage() const { return page_; }

private:
    LiquidCrystal_I2C lcd_;
    uint8_t           page_;

    void showSplash_() {
        lcd_.setCursor(0, 0);
        lcd_.print(F(" SMART BLIND    "));
        lcd_.setCursor(0, 1);
        lcd_.print(F("   STICK  v2.0  "));
        delay(2000);
        lcd_.clear();
        lcd_.setCursor(0, 0);
        lcd_.print(F("Initializing... "));
        lcd_.setCursor(0, 1);
        lcd_.print(F("Please wait...  "));
        delay(1500);
    }
};

// =============================================
//  SMART BLIND STICK CONTROLLER CLASS
// =============================================
class SmartBlindStick {
public:
    SmartBlindStick()
        : ultrasonic_(Pins::TRIG, Pins::ECHO),
          waterSensor_(Pins::WATER_AO, Pins::WATER_DO),
          buzzer_(Pins::BUZZER),
          gps_(Pins::GPS_RX, Pins::GPS_TX),
          gsm_(Pins::GSM_RX, Pins::GSM_TX),
          lastAlertTime_(0),
          lastSMSTime_(0),
          lastDisplayTime_(0) {}

    void begin() {
        Serial.begin(9600);
        pinMode(Pins::PANIC_BUTTON, INPUT_PULLUP);

        display_.begin();
        buzzer_.begin();
        buzzer_.startupBeep();

        ultrasonic_.begin();
        waterSensor_.begin();

        gsm_.begin();
        gps_.begin();

        Serial.println(F("=== Smart Blind Stick Ready ==="));
    }

    void run() {
        // 1. Update GPS
        gps_.update();

        // 2. Read sensors
        uint16_t distance = ultrasonic_.measure();
        waterSensor_.read();

        // 3. Handle alerts
        handleAlerts_(distance);

        // 4. Panic button check
        if (digitalRead(Pins::PANIC_BUTTON) == LOW) {
            delay(Config::DEBOUNCE_DELAY);
            if (digitalRead(Pins::PANIC_BUTTON) == LOW) {
                sendEmergencySMS_(AlertReason::PANIC);
                buzzer_.panicBeep();
            }
        }

        // 5. Auto water SMS (every 2 min)
        if (waterSensor_.isWetDetected() &&
            millis() - lastSMSTime_ > Config::AUTO_SMS_INTERVAL) {
            sendEmergencySMS_(AlertReason::WATER);
            lastSMSTime_ = millis();
        }

        // 6. LCD update
        if (millis() - lastDisplayTime_ >= Config::LCD_UPDATE_INTERVAL) {
            updateDisplay_(distance);
            lastDisplayTime_ = millis();
            display_.nextPage();
        }

        // 7. Debug
        debugPrint_(distance);

        delay(80);
    }

private:
    UltrasonicSensor ultrasonic_;
    WaterSensor      waterSensor_;
    Buzzer           buzzer_;
    GPSManager       gps_;
    GSMManager       gsm_;
    DisplayManager   display_;

    unsigned long lastAlertTime_;
    unsigned long lastSMSTime_;
    unsigned long lastDisplayTime_;

    // ---- Alert Logic ----
    void handleAlerts_(uint16_t distance) {
        unsigned long now = millis();

        if (waterSensor_.isWetDetected()) {
            if (now - lastAlertTime_ >= 600) {
                buzzer_.tripleBeep();
                lastAlertTime_ = now;
            }
            return;
        }

        ObstacleLevel level = ultrasonic_.getLevel();

        if (level == ObstacleLevel::DANGER) {
            if (now - lastAlertTime_ >= 200) {
                buzzer_.beep(100);
                lastAlertTime_ = now;
            }
        } else if (level == ObstacleLevel::WARNING) {
            if (now - lastAlertTime_ >= 450) {
                buzzer_.beep(120);
                lastAlertTime_ = now;
            }
        } else if (level == ObstacleLevel::CAUTION) {
            if (now - lastAlertTime_ >= 900) {
                buzzer_.beep(100);
                lastAlertTime_ = now;
            }
        } else {
            buzzer_.off();
        }
    }

    // ---- Display Logic ----
    void updateDisplay_(uint16_t distance) {
        switch (display_.getPage()) {
            case 0:
                display_.showObstaclePage(distance, ultrasonic_.getLevelString());
                break;
            case 1:
                display_.showWaterPage(waterSensor_.getStatusString(),
                                       waterSensor_.getValue());
                break;
            case 2:
                display_.showGPSPage(gps_.isFixed(), gps_.getLat(),
                                     gps_.getLng(), gps_.getSatCount());
                break;
        }
    }

    // ---- SMS Logic ----
    void sendEmergencySMS_(AlertReason reason) {
        String msg = "";

        if (reason == AlertReason::PANIC) {
            msg = F("EMERGENCY! Blind stick user needs HELP!\n");
        } else {
            msg = F("ALERT: Water/Rain detected near user!\n");
        }

        if (gps_.isFixed()) {
            msg += F("Location:\nLat: ");
            msg += String(gps_.getLat(), 6);
            msg += F("\nLng: ");
            msg += String(gps_.getLng(), 6);
            msg += F("\nMap: https://maps.google.com/?q=");
            msg += String(gps_.getLat(), 6);
            msg += F(",");
            msg += String(gps_.getLng(), 6);
        } else {
            msg += F("GPS not fixed. Location unavailable.");
        }

        gsm_.sendSMS(Config::CAREGIVER_NUMBER, msg);
        buzzer_.confirmBeep();
    }

    // ---- Serial Debug ----
    void debugPrint_(uint16_t distance) {
        Serial.print(F("Dist: "));
        Serial.print(distance);
        Serial.print(F("cm | Water: "));
        Serial.print(waterSensor_.getValue());
        Serial.print(F(" ("));
        Serial.print(waterSensor_.getStatusString());
        Serial.print(F(") | GPS: "));
        if (gps_.isFixed()) {
            Serial.print(gps_.getLat(), 6);
            Serial.print(F(", "));
            Serial.print(gps_.getLng(), 6);
        } else {
            Serial.print(F("No Fix"));
        }
        Serial.println();
    }
};

// =============================================
//  GLOBAL INSTANCE
// =============================================
SmartBlindStick stick;

// =============================================
//  ARDUINO ENTRY POINTS
// =============================================
void setup() {
    stick.begin();
}

void loop() {
    stick.run();
}
