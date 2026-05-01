#define TRIG_PIN 9
#define ECHO_PIN 10
#define BUZZER_PIN 6

long duration;
int distance;

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  // Send ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo
  duration = pulseIn(ECHO_PIN, HIGH);

  // Convert to distance (cm)
  distance = duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.println(distance);

  // Alert logic
  if (distance > 0 && distance <= 20) {
    tone(BUZZER_PIN, 1000); // fast beep (very close)
    delay(100);
  }
  else if (distance > 20 && distance <= 50) {
    tone(BUZZER_PIN, 800); // medium beep
    delay(300);
  }
  else if (distance > 50 && distance <= 100) {
    tone(BUZZER_PIN, 500); // slow beep
    delay(600);
  }
  else {
    noTone(BUZZER_PIN); // no obstacle
  }

  delay(50);
}
