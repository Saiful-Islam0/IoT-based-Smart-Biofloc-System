// firmware/main.ino
#include <Arduino.h>
#include <WiFi.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// -------- WiFi & Firebase ----------
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "https://your-project-default-rtdb.region.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;
unsigned long sendDataPrevMillis = 0;
unsigned long controlPollPrevMillis = 0;

// -------- Sensors & pins ----------
#define DHT_PIN   15
#define DHTTYPE   DHT22
DHT dht(DHT_PIN, DHTTYPE);

#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTempSensor(&oneWire);

// ADC pins / dividers
#define PH_PIN           39   // VN (ADC1_CH3)
#define TURB_PIN         34   // ADC1_CH6
#define WATER_ANALOG_PIN 35   // ADC1_CH7

const float VREF = 3.3;
const int ADC_RES = 4095;

// Relays (active LOW)
#define RELAY_PUMP_IN   16
#define RELAY_PUMP_OUT  17
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// Control flags
bool manualMode = false;
bool manualPumpIn = false;
bool manualPumpOut = false;

// Water-level thresholds (volts)
const float WL_LOW_TH  = 1.0;
const float WL_HIGH_TH = 2.0;

int analogReadAvg(int pin, int samples = 10) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (int)(sum / samples);
}

float analogToVoltage(int value) {
  return (value / (float)ADC_RES) * VREF;
}

// conversions (tune later)
float ph_offset = 0.0;
float turbidity_factor = 1.0;

float calcPH(float voltage) {
  return 7.0 + ((1.65f - voltage) / 0.18f) + ph_offset;
}
float calcTurbidity(float voltage) {
  return voltage * 100.0f * turbidity_factor;
}

void setRelay(uint8_t pin, bool on) {
  digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
}

void pollControlFromFirebase() {
  String base = "/devices/esp32_1/control";

  if (Firebase.RTDB.getBool(&fbdo, base + "/manualMode")) {
    manualMode = fbdo.to<bool>();
  }
  if (Firebase.RTDB.getBool(&fbdo, base + "/pumpIn")) {
    manualPumpIn = fbdo.to<bool>();
  }
  if (Firebase.RTDB.getBool(&fbdo, base + "/pumpOut")) {
    manualPumpOut = fbdo.to<bool>();
  }
}

void updatePumps(float vWL) {
  bool pumpIn  = false;
  bool pumpOut = false;

  if (manualMode) {
    pumpIn  = manualPumpIn;
    pumpOut = manualPumpOut;
  } else {
    if (vWL < WL_LOW_TH) pumpIn = true;
    else if (vWL > WL_HIGH_TH) pumpOut = true;
  }

  if (pumpIn && pumpOut) pumpOut = false;

  setRelay(RELAY_PUMP_IN,  pumpIn);
  setRelay(RELAY_PUMP_OUT, pumpOut);

  Serial.printf("[PUMP] WL=%.2f V | mode=%s | IN=%s OUT=%s\n", vWL, manualMode?"MANUAL":"AUTO",
                pumpIn?"ON":"OFF", pumpOut?"ON":"OFF");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // sensors
  pinMode(DHT_PIN, INPUT_PULLUP);
  dht.begin();
  waterTempSensor.begin();

  analogReadResolution(12);
  analogSetPinAttenuation(PH_PIN, ADC_11db);
  analogSetPinAttenuation(TURB_PIN, ADC_11db);
  analogSetPinAttenuation(WATER_ANALOG_PIN, ADC_11db);

  // relays
  pinMode(RELAY_PUMP_IN, OUTPUT);
  pinMode(RELAY_PUMP_OUT, OUTPUT);
  setRelay(RELAY_PUMP_IN, false);
  setRelay(RELAY_PUMP_OUT, false);

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  Serial.println("Signing up to Firebase...");
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Signup OK");
    signupOK = true;
  } else {
    Serial.printf("Signup failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase ready");
}

void loop() {
  float airTemp = dht.readTemperature();
  float airHum  = dht.readHumidity();

  waterTempSensor.requestTemperatures();
  float waterTemp = waterTempSensor.getTempCByIndex(0);

  int rawPH   = analogReadAvg(PH_PIN);
  int rawTurb = analogReadAvg(TURB_PIN);
  int rawWL   = analogReadAvg(WATER_ANALOG_PIN);

  float vPH   = analogToVoltage(rawPH);
  float vTurb = analogToVoltage(rawTurb);
  float vWL   = analogToVoltage(rawWL);

  float phVal   = calcPH(vPH);
  float turbVal = calcTurbidity(vTurb);

  bool waterPresent = (vWL > 1.5);

  // Update pump states
  updatePumps(vWL);

  // Serial debug
  Serial.println("==== SENSORS ====");
  Serial.printf("Air T: %.2f C | Hum: %.2f %%\n", airTemp, airHum);
  Serial.printf("Water T: %.2f C\n", waterTemp);
  Serial.printf("pH raw=%d V=%.3f pH=%.2f\n", rawPH, vPH, phVal);
  Serial.printf("Turb raw=%d V=%.3f turb=%.1f\n", rawTurb, vTurb, turbVal);
  Serial.printf("Water raw=%d V=%.3f present=%s\n", rawWL, vWL, waterPresent?"YES":"NO");

  // send to firebase every 10s
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 10000 || sendDataPrevMillis==0)) {
    sendDataPrevMillis = millis();
    String base = "/devices/esp32_1";

    Firebase.RTDB.setFloat(&fbdo, base + "/airTemp", airTemp);
    Firebase.RTDB.setFloat(&fbdo, base + "/airHum",  airHum);
    Firebase.RTDB.setFloat(&fbdo, base + "/waterTemp", waterTemp);
    Firebase.RTDB.setFloat(&fbdo, base + "/pH", phVal);
    Firebase.RTDB.setFloat(&fbdo, base + "/pHVoltage", vPH);
    Firebase.RTDB.setFloat(&fbdo, base + "/turbidity", turbVal);
    Firebase.RTDB.setFloat(&fbdo, base + "/turbVoltage", vTurb);
    Firebase.RTDB.setBool(&fbdo, base + "/waterPresent", waterPresent);
    Firebase.RTDB.setInt(&fbdo, base + "/waterRaw", rawWL);
    Firebase.RTDB.setFloat(&fbdo, base + "/waterVoltage", vWL);
    Firebase.RTDB.setInt(&fbdo, base + "/timestamp_ms", millis());

    String cbase = base + "/control";
    Firebase.RTDB.setBool(&fbdo, cbase + "/manualMode", manualMode);
    Firebase.RTDB.setBool(&fbdo, cbase + "/pumpInState", digitalRead(RELAY_PUMP_IN) == RELAY_ON);
    Firebase.RTDB.setBool(&fbdo, cbase + "/pumpOutState", digitalRead(RELAY_PUMP_OUT) == RELAY_ON);

    Serial.println("Pushed to Firebase");
  }

  // Poll control flags from Firebase every ~3s
  if (Firebase.ready() && signupOK && (millis() - controlPollPrevMillis > 3000 || controlPollPrevMillis==0)) {
    controlPollPrevMillis = millis();
    pollControlFromFirebase();
  }

  delay(500);
}
