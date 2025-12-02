# IoT-based-Smart-Biofloc-System — Smart Water Quality Monitoring & Pump Control

**Biofloc IoT** is an affordable end-to-end system for aquaculture (biofloc) that:
- Reads pH, turbidity, water temperature (DS18B20), air temp & humidity (DHT22), and water level.
- Displays values locally on an SSD1306 OLED and remotely on a live web dashboard.
- Sends data to **Firebase Realtime Database**.
- Controls two 5V pumps (fill / drain) via a 4-channel relay.
- Supports **AUTO** (auto-level control) and **MANUAL** (remote override) modes.
- Provides sensor health monitoring and alerts for missing data.

---

## Demo & Screenshots

*(Replace `images/dashboard.png` with your screenshot)*

![dashboard](images/dashboard.png)

*(Replace `images/schematic.png` with your wiring diagram)*

![schematic](images/schematic.png)

---

## Features

- Real-time telemetry to Firebase RTDB.
- Dashboard with live charts (Chart.js) and pump controls.
- Auto/manual pump logic with hysteresis to avoid rapid toggling.
- Voltage-divider protection for analog inputs.
- Sensor health detection (offline / missing data).
- Ready for ML/forecasting extension.

---

## Hardware

Minimum hardware list:
- ESP32 (dev board with Type-C)
- SSD1306 I2C OLED
- DHT22 (air temp & humidity)
- DS18B20 (water temperature)
- pH sensor module (analog)
- Turbidity sensor module (analog)
- Water level sensor (analog)
- 4-channel Relay (SRD-05VDC)
- 5V adapter (2A recommended) for relay and pumps
- 5V DC pumps (two) — or relays for mains pumps (if using mains, consult an electrician)
- Resistors: 4.7kΩ, 10kΩ, 1kΩ, 2kΩ (for pull-ups and voltage dividers)
- Wires, breadboard / terminal block, case

---

## Wiring summary (short)
- OLED: SDA -> GPIO21, SCL -> GPIO22, VCC -> 5V, GND -> GND
- DHT22: DATA -> GPIO15, VCC -> 3.3V, GND -> GND (use pull-up if raw)
- DS18B20: DATA -> GPIO4, VCC -> 3.3V, GND -> GND, 4.7kΩ pull-up between DATA and VCC
- pH: Signal -> ADC (GPIO39 / VN) via 1kΩ -> pin -> 2kΩ -> GND divider, VCC -> 5V, GND -> common
- Turbidity: Signal -> ADC (GPIO34) via 1kΩ/2kΩ divider, VCC -> 5V
- Water level: Signal -> ADC (GPIO35) via 1kΩ/2kΩ divider
- Relay VCC -> 5V (external 5V), GND -> common GND, IN1 -> GPIO16, IN2 -> GPIO17
- Relay COM -> 5V+, Relay NO -> Pump +, Pump - -> 5V-

> Important: share a common ground for ESP32, relay, sensors, and pump power negative.

---

## Firebase setup

1. Create a Firebase project in the Firebase console.
2. Enable Realtime Database and choose location same as your project.
3. Set rules (for testing, you may allow open reads/writes; remove before production).
4. Use **Anonymous Authentication** in Firebase for the ESP32 to sign in (or set up a service account).
5. Copy your Firebase config keys and update `dashboard/index.html` and `firmware/main.ino` WiFi + Firebase placeholders.

**Important:** Firebase API keys are not secret. But protect any server credentials if you later create a backend.

---

## Install & Run

### 1. Flash ESP32 firmware
- Open `firmware/main.ino` in Arduino IDE (or PlatformIO).
- Install libraries:
  - Firebase ESP Client (mobizt)
  - DHT sensor library (Adafruit)
  - OneWire
  - DallasTemperature
  - (ArduinoJson is not required here)
- Update:
  - `WIFI_SSID` and `WIFI_PASSWORD`
  - `API_KEY` and `DATABASE_URL` (your Firebase config)
- Select the correct board and COM port, then Upload.

### 2. Deploy dashboard (static)
Option A — local:
- Open `dashboard/index.html` in browser (use a local server for ES modules).
  - Quick: `npx serve dashboard` or use VSCode Live Server.

Option B — GitHub Pages:
- Push repo to GitHub.
- In repo settings -> GitHub Pages -> Select `dashboard/` folder as source (or use root) and enable.

Update `firebaseConfig` object in `dashboard/index.html` with your Firebase config.

---

## File descriptions

- `firmware/main.ino` — Main Arduino sketch for ESP32 that reads sensors, controls relays, and sends data to Firebase.
- `dashboard/index.html` — Single-file frontend (Tailwind + Chart.js) that connects to Firebase and shows live data, charts & pump controls.
- `images/` — Schematic and screenshots for the README.
- `docs/quick_start.md` — Optional condensed setup.

---

## Future work / Roadmap

- Add ML model for forecast and anomaly detection (server or edge).
- Store time-series logs under `/logs/{device}/{timestamp}` for long-term historical analysis.
- Add authentication and role-based control (farmer, admin).
- Add predictive pump scheduling and energy optimization.
- Expand to multi-tank, multi-farm dashboard.

---

## License
This project is under the **MIT License**. See [LICENSE](LICENSE).

---

## Contributing
- Fork the repo, create a feature branch, push, then open a PR.
- For major changes, open an issue first to discuss.

---

## Contact
Saiful Islam — study.saiful1@gmail.com  
Project: Biofloc IoT
