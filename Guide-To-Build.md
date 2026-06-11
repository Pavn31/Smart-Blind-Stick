# 🔨 Build Guide

This guide explains how to assemble and test the Smart Monitoring System using Arduino UNO, HC-SR04, Water Sensor, GPS NEO-6M, SIM800L, and a Buzzer.

---

## 📦 Components Required

| Component | Quantity |
|------------|------------|
| Arduino UNO | 1 |
| HC-SR04 Ultrasonic Sensor | 1 |
| Water/Rain Sensor | 1 |
| GPS NEO-6M | 1 |
| SIM800L GSM Module | 1 |
| Buzzer | 1 |
| 7V–12V Battery | 1 |
| 5V Buck Converter | 1 |
| Jumper Wires | As Required |

---

## 🖼 Circuit Diagram

![Circuit Diagram](Circuit-Diagram.png)

---

## ⚡ Step 1: Prepare the Power Supply

1. Connect the battery to the buck converter.
2. Adjust the converter output to **5V**.
3. Verify the voltage using a multimeter.
4. Connect the regulated 5V output to all modules.

> **Warning:** SIM800L requires a stable power supply and may draw high current during transmission.

---

## 📏 Step 2: Connect the HC-SR04 Ultrasonic Sensor

| HC-SR04 Pin | Arduino UNO |
|-------------|-------------|
| VCC | 5V |
| GND | GND |
| TRIG | D9 |
| ECHO | D10 |

**Purpose:** Detects nearby obstacles and measures distance.

---

## 🌧 Step 3: Connect the Water Sensor

| Water Sensor Pin | Arduino UNO |
|------------------|-------------|
| VCC | 5V |
| GND | GND |
| AO | A0 |
| DO | D2 |

**Purpose:** Detects water or rain.

---

## 📍 Step 4: Connect the GPS Module

| GPS Pin | Arduino UNO |
|----------|-------------|
| VCC | 5V |
| GND | GND |
| TX | D4 (RX) |
| RX | D3 (TX) |

**Purpose:** Provides real-time latitude and longitude coordinates.

---

## 📲 Step 5: Connect the GSM Module

| SIM800L Pin | Arduino UNO |
|-------------|-------------|
| VCC | 5V |
| GND | GND |
| TXD | D7 (RX) |
| RXD | D8 (TX) |

**Purpose:** Sends SMS alerts and notifications.

---

## 🔊 Step 6: Connect the Buzzer

| Buzzer Pin | Arduino UNO |
|------------|-------------|
| + | D6 |
| - | GND |

**Purpose:** Generates audible alerts.

---

## 💻 Step 7: Upload the Arduino Code

1. Install Arduino IDE.
2. Connect Arduino UNO to your computer.
3. Open the project `.ino` file.
4. Select:
   - **Board:** Arduino UNO
   - **Port:** Correct COM Port
5. Install required libraries:
   - TinyGPS++
   - SoftwareSerial
6. Click **Upload**.

---

## 🧪 Step 8: Testing

### Obstacle Detection Test

1. Power on the system.
2. Place an object in front of the ultrasonic sensor.
3. Verify that the buzzer activates.

### Water Detection Test

1. Place water on the sensor plate.
2. Verify that the alert is triggered.

### GPS Test

1. Move outdoors.
2. Wait for GPS satellite lock.
3. Verify latitude and longitude values.

### GSM Test

1. Insert an active SIM card.
2. Ensure network coverage is available.
3. Trigger an alert.
4. Verify SMS delivery.

---

## 🛠 Troubleshooting

### SIM800L Not Working

- Check SIM card activation.
- Verify GSM signal availability.
- Ensure stable power supply.
- Verify TX/RX wiring.

### GPS Not Receiving Coordinates

- Move outdoors.
- Wait several minutes for GPS lock.
- Check module connections.

### Ultrasonic Sensor Not Detecting Objects

- Verify TRIG and ECHO wiring.
- Check power connections.
- Test with a nearby object.

### Water Sensor Not Responding

- Verify AO and DO connections.
- Adjust sensor sensitivity.

---

## ✅ Final Assembly Checklist

- [ ] Arduino Code Uploaded
- [ ] HC-SR04 Working
- [ ] Water Sensor Working
- [ ] GPS Coordinates Received
- [ ] GSM SMS Alerts Working
- [ ] Buzzer Working
- [ ] Stable 5V Supply Verified

---

## 🎉 Project Ready

The Smart Monitoring System is now fully assembled and ready for operation.
