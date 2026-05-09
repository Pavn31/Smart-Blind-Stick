🦯 Smart Blind Stick
> An IoT-enabled assistive device for visually impaired individuals, featuring obstacle detection, water sensing, GPS tracking, and real-time smartphone alerts via GSM and Blynk.
![Arduino](https://img.shields.io/badge/Platform-Arduino%20Nano-00979D?style=flat-square&logo=arduino)
![Language](https://img.shields.io/badge/Language-Embedded%20C%2FC%2B%2B-blue?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-green?style=flat-square)
![Status](https://img.shields.io/badge/Status-Active-brightgreen?style=flat-square)
---
📋 Table of Contents
Overview
Features
Hardware Requirements
Software Requirements
Circuit Diagram
Pin Configuration
Project Structure
Installation & Setup
Blynk App Configuration
Building & Uploading
Usage
Troubleshooting
Contributing
License
---
Overview
The Smart Blind Stick is an Arduino-based assistive device designed to enhance the mobility and safety of visually impaired individuals. It combines ultrasonic obstacle detection, water/rain sensing, GPS location tracking, and GSM-based emergency alerts — all integrated with the Blynk mobile application for real-time monitoring.
---
Features
🔊 Obstacle Detection — Detects objects within 2 cm to 400 cm using HC-SR04 ultrasonic sensor; triggers buzzer alerts
💧 Water/Rain Detection — Alerts the user when the path ahead has water or wet surfaces
📍 GPS Tracking — Real-time location tracking using NEO-6M GPS module
📱 Smartphone Integration — Live data pushed to the Blynk app (iOS / Android)
📡 GSM Alerts — Sends SMS with GPS coordinates to a caregiver in emergency situations
⚡ Low Power Design — Runs on a 9V or Li-ion rechargeable battery
---
Hardware Requirements
#	Component	Specification
1	Microcontroller	Arduino Nano (ATmega328P) — 32KB Flash, 5V
2	Ultrasonic Sensor	HC-SR04 — Range: 2 cm to 400 cm
3	Water/Rain Sensor	Generic Rain Sensor Module
4	GPS Module	NEO-6M (UART interface)
5	GSM Module	SIM800L or equivalent
6	SIM Card	Active network connection
7	Buzzer	Active or Passive Buzzer
8	Display (Optional)	Small OLED / LCD for distance readings
9	Power Supply	9V Battery or Li-ion Rechargeable Battery
10	USB Cable	For programming via computer
> **Note:** The SIM800L operates at 3.7V–4.2V. Use a proper voltage regulator or LiPo battery to power it separately to avoid brownouts on the Arduino.
---
Software Requirements
Development Environment
Tool	Version
Arduino IDE	1.8 or above
Blynk App	Latest (iOS / Android)
Operating System	Windows 7/10/11, macOS, or Linux
RAM	Minimum 4 GB
Free Storage	Minimum 500 MB
Programming Language
Embedded C / C++ (Arduino Framework)
Required Libraries
Install all libraries via Arduino IDE → Tools → Manage Libraries, or download manually from GitHub.
Library	Purpose	Install via
`SoftwareSerial`	Serial communication for GPS & GSM	Built-in (Arduino IDE)
`TinyGPS++`	GPS NMEA sentence parsing	Library Manager
`Blynk`	Smartphone app integration	Library Manager
---
Circuit Diagram
```
                          ┌─────────────────────┐
                          │     Arduino Nano      │
                          │                       │
   HC-SR04 ──────────────►│ D2 (Trig)  D3 (Echo) │
   Rain Sensor ──────────►│ A0 (Analog Input)     │
   Buzzer ───────────────►│ D8                    │
   GPS TX ───────────────►│ D4 (RX Software)      │
   GPS RX ◄───────────────│ D5 (TX Software)      │
   GSM TX ───────────────►│ D6 (RX Software)      │
   GSM RX ◄───────────────│ D7 (TX Software)      │
   Display SDA ──────────►│ A4                    │
   Display SCL ──────────►│ A5                    │
                          │                       │
   VCC (5V) ─────────────►│ 5V                    │
   GND ──────────────────►│ GND                   │
                          └─────────────────────┘
```
> Refer to `docs/circuit_diagram.png` for the full schematic.
---
Pin Configuration
Arduino Pin	Connected To	Mode
D2	HC-SR04 TRIG	OUTPUT
D3	HC-SR04 ECHO	INPUT
D4	GPS Module TX	SoftwareSerial RX
D5	GPS Module RX	SoftwareSerial TX
D6	GSM Module TX	SoftwareSerial RX
D7	GSM Module RX	SoftwareSerial TX
D8	Buzzer	OUTPUT
A0	Rain Sensor (Analog)	INPUT
A4	Display SDA (optional)	I2C
A5	Display SCL (optional)	I2C
5V	VCC Rail	Power
GND	Ground Rail	Power
---
Project Structure
```
smart-blind-stick/
│
├── src/
│   └── smart_blind_stick.ino      # Main Arduino sketch
│
├── docs/
│   ├── circuit_diagram.png        # Full wiring schematic
│   ├── pin_configuration.md       # Detailed pin mapping
│   └── blynk_setup.md             # Blynk dashboard setup guide
│
├── libraries/                     # Local copies of required libraries (optional)
│   ├── TinyGPSPlus/
│   └── Blynk/
│
├── assets/
│   └── demo.gif                   # Working demo
│
├── .gitignore
├── LICENSE
└── README.md
```
---
Installation & Setup
Step 1 — Clone the Repository
```bash
git clone https://github.com/<your-username>/smart-blind-stick.git
cd smart-blind-stick
```
Step 2 — Install Arduino IDE
Download from https://www.arduino.cc/en/software and install for your OS.
Step 3 — Install Required Libraries
Open Arduino IDE and go to Tools → Manage Libraries, then search for and install:
```
TinyGPSPlus    by Mikal Hart
Blynk          by Volodymyr Shymanskyy
```
`SoftwareSerial` is built into the Arduino IDE — no installation needed.
Step 4 — Configure Blynk Auth Token
Create a new project in the Blynk app.
Select Arduino Nano as the device.
Copy the Auth Token sent to your email.
Open `src/smart_blind_stick.ino` and replace the placeholder:
```cpp
// Replace with your Blynk Auth Token
char auth[] = "YOUR_BLYNK_AUTH_TOKEN";
```
Step 5 — Configure GSM Emergency Contact
In the sketch, set the caregiver's phone number:
```cpp
// Replace with caregiver's phone number (include country code)
String emergencyNumber = "+91XXXXXXXXXX";
```
---
Blynk App Configuration
Virtual Pin	Widget	Purpose
V1	Value Display	Obstacle Distance (cm)
V2	LED Widget	Water Detected Indicator
V3	Map Widget	Real-time GPS Location
V4	Notification	Emergency Alert
> Detailed setup with screenshots is available in `docs/blynk_setup.md`.
---
Building & Uploading
Connect your Arduino Nano to the computer via USB.
In Arduino IDE:
Go to Tools → Board → Arduino Nano
Go to Tools → Processor → ATmega328P (Old Bootloader) (if upload fails, try without "Old Bootloader")
Select the correct Tools → Port (e.g., `COM3` on Windows, `/dev/ttyUSB0` on Linux/macOS)
Open `src/smart_blind_stick.ino`.
Click Verify (✓) to compile, then Upload (→) to flash.
Open Serial Monitor at `9600 baud` to debug output.
---
Usage
Power on the stick using the battery.
Open the Blynk app on your smartphone — the device will connect automatically.
The buzzer will beep at varying frequencies based on obstacle proximity:
Fast beeps — Object closer than 30 cm
Slow beeps — Object between 30 cm and 100 cm
No beep — Path is clear
If water is detected, the buzzer gives a distinct alarm pattern.
In an emergency, press the emergency button (if wired) to send an SMS with GPS coordinates to the caregiver.
---
Troubleshooting
Problem	Possible Cause	Fix
Upload fails	Wrong board/port selected	Verify Tools → Board and Port
GPS not getting fix	Outdoors signal needed	Move outdoors; wait 2–3 minutes for cold start
GSM not sending SMS	SIM not active / low signal	Check SIM balance and signal strength
Blynk not connecting	Wrong auth token / no Wi-Fi	Verify token; GSM-based Blynk needs data plan
Buzzer always on	Sensor wiring issue	Check HC-SR04 VCC and GND connections
Brownout / reset loop	SIM800L underpowered	Power GSM module from a separate 4V supply
---
Contributing
Contributions are welcome! To contribute:
Fork the repository
Create a feature branch: `git checkout -b feature/your-feature-name`
Commit your changes: `git commit -m "Add: your feature description"`
Push to the branch: `git push origin feature/your-feature-name`
Open a Pull Request
Please follow the existing code style and add comments for any new functionality.
---
License
This project is licensed under the MIT License — see the LICENSE file for details.
---
<p align="center">Made with ❤️ to empower the visually impaired community</p>
