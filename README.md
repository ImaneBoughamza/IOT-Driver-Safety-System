# 🚗 Driver Safety Monitoring System

> A low-cost, embedded IoT system that detects driver drowsiness, speeding, and lane departure in real time — using rule-based logic instead of machine learning, so it runs on affordable, resource-constrained hardware.

Built for **EGR 3328 – Embedded Systems**, School of Science and Engineering, Al Akhawayn University in Ifrane (Spring 2026).

---

## 📽️ Demo Videos

- 🎥 [IR Drowsiness Detection](https://youtu.be/36v2UiFx-M4)
- 🎥 [GPS Speed Parsing](https://youtu.be/Vc8wlyAkMuA)
- 🎥 [ESP32-CAM Lane Detection](https://youtu.be/qXxhCIqxOsU)

---

## 💡 Why This Project

Advanced driver-assistance systems (ADAS) rely on expensive LiDAR, radar, and ML pipelines — putting real-time driver safety monitoring out of reach for budget vehicles and older cars. This project asks: **how much of that safety value can be delivered with cheap sensors and deterministic logic alone?**

The answer: three independent hazard-detection systems, unified on a single microcontroller, for under 400 MAD in components.

---

## ✨ Key Features

- 😴 **Drowsiness detection** — an IR sensor tracks eye closure; an alert triggers only after **1.5 seconds** of sustained closure, filtering out normal blinks (150–400ms).
- 🚦 **Speed monitoring** — parses live `$GPRMC` NMEA sentences from a GPS module over UART, converts knots to km/h, and flags violations against a configurable threshold.
- 🛣️ **Lane departure detection** — an ESP32-CAM analyzes grayscale pixel brightness in the road-facing frame, entirely rule-based, no ML required.
- 🚨 **Real-time alerts** — a passive buzzer plus three LEDs (🔵 safe · 🟡 drowsiness · 🔴 speeding) give instant, mutually-exclusive feedback.
- ☁️ **Remote cloud monitoring** — every hazard event is published via Particle Cloud webhooks to a live ThingSpeak dashboard, viewable from any browser.
- ⚡ **10 Hz sampling** — a 100ms non-blocking loop keeps all three subsystems responsive simultaneously.

---

## 🛠️ Tech Stack

<p>
  <img src="https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/Particle_Photon-53B0EF?style=for-the-badge&logo=particle&logoColor=white" />
  <img src="https://img.shields.io/badge/ESP32--CAM-E7352C?style=for-the-badge&logo=espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/ThingSpeak-1E90FF?style=for-the-badge" />
  <img src="https://img.shields.io/badge/UART%2FGPIO-333333?style=for-the-badge" />
</p>

**Hardware:** Particle Photon · NEO-6M GPS Module · IR Obstacle Sensor · ESP32-CAM (OV2640) · FTDI USB-to-Serial Adapter · Passive Buzzer · RGB Status LEDs

---

## 🏗️ System Architecture

Two independent circuits, connected only over Wi-Fi:

```
┌────────────────────────┐        ┌────────────────────────┐
│   Photon Breadboard      │        │   ESP32-CAM Circuit      │
│   • NEO-6M GPS (UART)    │  Wi-Fi │   • OV2640 Camera         │
│   • IR Sensor (GPIO)     │◄──────►│   • Lane detection logic  │
│   • Buzzer + 3 LEDs      │        │   • Powered via FTDI      │
└───────────┬─────────────┘        └────────────────────────┘
            │ Particle.publish()
            ▼
   ┌──────────────────┐
   │  Particle Cloud    │
   │  (Webhook trigger)  │
   └─────────┬──────────┘
             ▼
   ┌──────────────────┐
   │   ThingSpeak       │
   │  4 live data fields │
   │  (speed, drowsy,    │
   │   lane, alert)      │
   └──────────────────┘
```

**Decision engine logic (runs every 100ms on the Photon):**
1. Read GPS speed via UART → flag if above threshold
2. Read IR sensor state → flag if eyes closed > 1.5s
3. Receive lane status from ESP32-CAM over Wi-Fi → flag if lane departure detected
4. If **any** flag is true → activate buzzer + corresponding LED, blue "safe" LED turns off
5. Publish all four fields to Particle Cloud → forwarded to ThingSpeak via webhook

---

## 🚀 Getting Started

### 1. Flash the Photon firmware
- Open [Particle Web IDE](https://build.particle.io) or Particle Workbench
- Paste in the Photon firmware (see `/firmware/photon`)
- Adjust thresholds if needed:
  ```cpp
  #define SPEED_THRESHOLD_KMH   20.0   // km/h
  #define DROWSY_THRESHOLD_MS   1500   // ms
  ```
- Flash to device

### 2. Flash the ESP32-CAM firmware
- In Arduino IDE, select board **"AI Thinker ESP32-CAM"**
- Paste in the camera firmware (see `/firmware/esp32cam`)
- Update Wi-Fi credentials:
  ```cpp
  const char* ssid = "YOUR_HOTSPOT";
  const char* password = "YOUR_PASSWORD";
  ```
- Connect `IO0` to `GND` to enter flash mode, upload, then disconnect and reset

### 3. Set up the cloud pipeline
- Create a [ThingSpeak](https://thingspeak.com) channel with 4 fields (speed, drowsiness, lane, alert)
- In the [Particle Console](https://console.particle.io), create a webhook forwarding your `Particle.publish()` events to the ThingSpeak Write API

Full wiring diagrams and a complete troubleshooting guide are included in the project report (Appendix A).

---

## 📊 Performance Results

| Subsystem | Expected | Observed |
|---|---|---|
| IR Sensor | Alert after 1.5s eye closure | ✅ Consistent, blinks correctly filtered |
| GPS Module | Accurate speed in km/h | ✅ Matched known values, ±2–3 km/h variance |
| ESP32-CAM | Detect lane markings | ✅ Reliable in good lighting; struggles in dim conditions |
| LED Indicators | Correct color per alert | ✅ All respond correctly, mutually exclusive |
| ThingSpeak | Data logged within 15s | ✅ All four fields update correctly |

---

## ⚠️ Known Limitations

- IR sensor requires precise physical positioning near the driver's eyes — not practical for real vehicle deployment as-is
- GPS loses signal in tunnels, parking garages, and dense urban areas
- Lane detection (pixel-brightness based) fails in poor lighting, rain, or on unmarked/worn roads
- No automatic failure detection if a sensor disconnects mid-operation
- ESP32-CAM circuit currently depends on a laptop + FTDI adapter for power, limiting portability

## 🔭 Future Work

- Fully portable camera module (Wi-Fi HTTP direct to Photon, no laptop dependency)
- Automatic failure detection & fallback (e.g., GPS-loss warning, IR-only fallback mode)
- Dynamic speed thresholds based on GPS-derived road type
- SD card local logging as an offline backup to ThingSpeak
- Migration from breadboard prototype to a custom PCB with vehicle-ready enclosure

---

## 📁 Repository Structure

```
driver-safety-monitoring-system/
├── firmware/
│   ├── photon/           → GPS parsing, IR detection, decision engine
│   └── esp32cam/         → Lane detection + Wi-Fi communication
├── docs/
│   └── project-report.pdf
└── README.md
```

---

## 📝 License

MIT
