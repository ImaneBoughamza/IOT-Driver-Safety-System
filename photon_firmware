// ============================================================
// Driver Safety Monitoring System — Photon Firmware
// GPS Speed Monitoring + IR Drowsiness Detection + Decision Engine
// ============================================================
#include "Particle.h"

#define IR_PIN         D2
#define LED_SAFE       D3   // Blue
#define LED_DROWSY     D4   // Yellow
#define LED_SPEED      D5   // Red
#define BUZZER_PIN     D6   // Passive Buzzer

#define DROWSY_THRESHOLD_MS   1500   // 1.5 seconds eyes closed
#define SPEED_THRESHOLD_KMH   20.0
#define BUZZER_FREQ           1000   // 1kHz tone
#define THINGSPEAK_INTERVAL   15000  // 15 seconds (ThingSpeak rate limit)
#define LANE_TIMEOUT_MS       10000  // 10 seconds without update = camera offline

// Drowsiness
unsigned long eyesClosedSince = 0;
bool eyesClosed       = false;
bool drowsinessAlert  = false;

// GPS
String nmeaBuffer  = "";
float currentSpeed = 0.0;
bool speedAlert     = false;

// Lane Departure (received from ESP32-CAM via cloud function)
bool laneAlert             = false;
unsigned long lastLaneUpdate = 0;
bool cameraOnline           = false;

// ThingSpeak
unsigned long lastPublish = 0;

// Cloud function to receive lane status from ESP32-CAM
int handleLaneAlert(String value) {
    laneAlert = (value == "1");
    lastLaneUpdate = millis();
    cameraOnline = true;
    Serial.printlnf("Lane status received: %s",
        laneAlert ? "DEPARTURE" : "OK");
    return 1;
}

float parseSpeed(String sentence) {
    int commaCount = 0;
    int start = 0;
    for (int i = 0; i < sentence.length(); i++) {
        if (sentence[i] == ',') {
            commaCount++;
            if (commaCount == 7) start = i + 1;
            if (commaCount == 8) {
                String speedStr = sentence.substring(start, i);
                if (speedStr.length() > 0) return speedStr.toFloat() * 1.852;
            }
        }
    }
    return -1;
}

void setup() {
    Serial.begin(9600);
    Serial1.begin(9600);

    pinMode(IR_PIN,     INPUT);
    pinMode(LED_SAFE,   OUTPUT);
    pinMode(LED_DROWSY, OUTPUT);
    pinMode(LED_SPEED,  OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(LED_SAFE,   HIGH);
    digitalWrite(LED_DROWSY, LOW);
    digitalWrite(LED_SPEED,  LOW);
    digitalWrite(BUZZER_PIN, LOW);

    // Register cloud function for ESP32-CAM lane alerts
    Particle.function("laneAlert", handleLaneAlert);

    Serial.println("=== Driver Safety Monitor ===");
}

void loop() {
    // --- GPS ---
    while (Serial1.available()) {
        char c = Serial1.read();
        if (c == '\n') {
            if (nmeaBuffer.startsWith("$GPRMC")) {
                float spd = parseSpeed(nmeaBuffer);
                if (spd >= 0) {
                    currentSpeed = spd;
                    speedAlert = (currentSpeed > SPEED_THRESHOLD_KMH);
                    Serial.printlnf("Speed: %.1f km/h %s", currentSpeed,
                        speedAlert ? "| SPEED ALERT" : "");
                }
            }
            nmeaBuffer = "";
        } else if (c != '\r') {
            nmeaBuffer += c;
        }
    }

    // --- IR Drowsiness ---
    int ir = digitalRead(IR_PIN);

    if (ir == HIGH) { // no reflection = eye closed
        if (!eyesClosed) {
            eyesClosed = true;
            eyesClosedSince = millis();
            Serial.println("Eyes closed...");
        }
        if (millis() - eyesClosedSince >= DROWSY_THRESHOLD_MS && !drowsinessAlert) {
            drowsinessAlert = true;
            Serial.println("DROWSINESS ALERT!");
        }
    } else { // reflection detected = eye open
        if (eyesClosed) Serial.println("Eyes open - alert cleared");
        eyesClosed       = false;
        drowsinessAlert  = false;
        eyesClosedSince  = 0;
    }

    // --- Camera Timeout Check ---
    if (cameraOnline && (millis() - lastLaneUpdate > LANE_TIMEOUT_MS)) {
        cameraOnline = false;
        laneAlert = false;
        Serial.println("WARNING: Camera offline - no update in 10s");
    }

    // --- LEDs ---
    bool anyAlert = drowsinessAlert || speedAlert || laneAlert;
    digitalWrite(LED_DROWSY, drowsinessAlert ? HIGH : LOW);
    digitalWrite(LED_SPEED,  speedAlert      ? HIGH : LOW);
    digitalWrite(LED_SAFE,   !anyAlert       ? HIGH : LOW);

    // --- Buzzer ---
    if (anyAlert) {
        tone(BUZZER_PIN, BUZZER_FREQ);
    } else {
        noTone(BUZZER_PIN);
    }

    // --- ThingSpeak Publishing ---
    if (millis() - lastPublish >= THINGSPEAK_INTERVAL) {
        lastPublish = millis();

        int overallAlert = (drowsinessAlert || speedAlert || laneAlert) ? 1 : 0;

        String data = String::format(
            "{\"field1\":%.1f,\"field2\":%d,\"field3\":%d,\"field4\":%d}",
            currentSpeed,
            drowsinessAlert ? 1 : 0,
            laneAlert ? 1 : 0,
            overallAlert
        );

        Particle.publish("thingspeak", data, PRIVATE);
        Serial.printlnf("Published to ThingSpeak: %s", data.c_str());
    }

    delay(100);
}
