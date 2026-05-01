***Before You Start***
*Make sure you have:*
-Arduino Nano

-HC-SR04 Ultrasonic Sensor

-Water/Rain Sensor
-GPS Module (NEO-6M)
-GSM Module (SIM800L)
-Buzzer
-Small Display (optional)
-Battery (9V or Li-ion)
-Jumper wires
Keep your laptop ready with Arduino IDE installed.

***Step 1: Set Up the Brain (Arduino Nano):***
*Think of the Arduino as the “brain” of your stick.*
-Place it on a breadboard (if you’re using one)
-Don’t connect power yet
-We’ll connect everything to this

***📡 Step 2: Connect Ultrasonic Sensor (Obstacle Detection):***
*This helps detect objects in front.*
-VCC → 5V on Arduino
-GND → GND
-TRIG → Any digital pin (e.g., D3)
-ECHO → Any digital pin (e.g., D2)
**👉 This sensor will measure distance and tell Arduino if something is close.**

***Step 3: Connect Water Sensor (Puddle Detection):***
*This helps detect water on the ground.*
-VCC → 5V
-GND → GND
-Analog OUT → A0 (Analog pin)
**👉 When it touches water, values change → Arduino triggers alert.**

***Step 4: Connect GPS Module (Location Tracking):***
*Used to get live location.*
-VCC → 5V
-GND → GND
-TX → Arduino RX (use SoftwareSerial, e.g., D4)
-RX → Arduino TX (e.g., D5)
**👉 GPS sends coordinates continuously.**

***Step 5: Connect GSM Module (SIM800L)***
Used to send SMS/location.
*⚠️ Important: SIM800L usually needs ~3.7–4.2V (not 5V directly)*
-VCC → External regulated power (NOT Arduino 5V)
-GND → Common GND
-TX → Arduino RX (e.g., D7)
-RX → Arduino TX (e.g., D6)
**👉 This sends alerts like “Help needed” with GPS location.**

***Step 6: Connect Buzzer (Alert System):***
-This gives sound alerts.
-Positive → Digital pin (e.g., D8)
-Negative → GND
**👉 Arduino will beep when obstacle/water is detected.**

***Step 7: Connect Display (Optional):***
*If you're using a small display (like OLED/LCD):*
-VCC → 5V
-GND → GND
-SDA → A4
-SCL → A5
**👉 Shows distance, GPS data, etc.**

***Step 8: Power the System:***
-Use 9V battery or Li-ion battery
**Connect:**
-VCC → VIN/5V (depending on setup)
-GND → GND
*👉 Make sure:*
-All GNDs are connected together (common ground)
-GSM has proper voltage (very important)

***Step 9: Upload Code:***
**In Arduino IDE:**
*1. Install libraries:*
-TinyGPS++
-SoftwareSerial
-Blynk (if using app)
*2. Write or paste your code:*
-Read ultrasonic distance
-Check water sensor value
-Read GPS location
-Send SMS via GSM
-Trigger buzzer
*3. Select:*
-Board: Arduino Nano
-Port: Correct COM port
*4. Click Upload*

***Step 10: Test Like a Human Would***
-Put your hand in front → Buzzer should beep
-Touch water sensor → Alert should trigger
-Check GPS → Should give coordinates
-Trigger emergency → GSM sends SMS

***Final Assembly (Make it a Stick)***
-Mount ultrasonic sensor at the front
-Fix water sensor near bottom
-Keep Arduino + modules in a small box
-Attach buzzer where sound is clear
-Secure battery safely
