/*
 * ============================================================================
 * FIRMWARE SMART GREENHOUSE ESP32 - INTEGRASI FULL & FAIL-SAFE ARCHITECTURE
 * Modul Sensor : DHT22 (Pin 4), Soil Moisture (Pin 34)
 * Aktuator     : Kipas (Pin 27), Pompa Mini (Pin 26)
 * Keamanan     : E-Stop Physical Switch (Pin 14)
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// ==========================================
// 1. PIN DEFINITION & HARDWARE CONFIG
// ==========================================
#define DHTPIN        4     // Pin Data DHT22
#define DHTTYPE       DHT22 // Tipe Sensor
#define SOIL_PIN      34    // Pin Analog Soil Sensor
#define FAN_PIN       27    // Pin Relay/Driver Kipas
#define PUMP_PIN      26    // Pin Relay/Driver Pompa
#define ESTOP_PIN     14    // Pin Emergency Stop Switch (INPUT_PULLUP)

const bool ACTUATOR_ACTIVE_LOW = true; // Set TRUE jika Active LOW (Relay Module), FALSE jika Active HIGH (Transistor)

// ==========================================
// 2. KONSTANTA KALIBRASI & HISTERESIS
// ==========================================
const float ADC_DRY           = 3298.0; // 0% Moisture
const float ADC_WET           = 1193.0; // 100% Moisture
const float ADC_AIR_THRESHOLD = 3380.0; // Threshold Sensor Udara/Tercabut

// Histeresis Kontrol
const float TEMP_FAN_ON       = 35.0; // Kipas ON > 35.0 C
const float TEMP_FAN_OFF      = 28.0; // Kipas OFF <= 28.0 C
const float MOISTURE_PUMP_ON  = 30.0; // Pompa ON < 30.0%
const float MOISTURE_PUMP_OFF = 60.0; // Pompa OFF >= 60.0%

// Safety & Timing
const unsigned long MAX_PUMP_ON_TIME = 10000; // Maksimal pompa nyala 10 detik
const unsigned long PUMP_COOLDOWN    = 15000; // Jeda istirahat pompa 15 detik
const unsigned long SENSOR_INTERVAL  = 1000;  // Interval pembacaan sensor (1s)
const unsigned long MQTT_RETRY_INT   = 5000;  // Interval percobaan reconnect MQTT (5s)

// ==========================================
// 3. NETWORK CONFIGURATION (NON-BLOCKING)
// ==========================================
const char* WIFI_SSID     = "MONITORING-ESP32"; // Ganti dengan SSID WiFi/Hotspot Anda
const char* WIFI_PASSWORD = "esp32monitor";     // Ganti dengan Password WiFi Anda
const char* MQTT_SERVER   = "broker.emqx.io";   // Cloud Public Broker (atau IP Mosquitto lokal: 192.168.1.100)
const int   MQTT_PORT     = 1883;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==========================================
// 4. STRUKTUR DATA FILTER SENSOR
// ==========================================
#define MEDIAN_SIZE 15
#define MOVING_AVG_SIZE 5

int medianBuffer[MEDIAN_SIZE];
int medianIndex = 0;
float movingAvgBuffer[MOVING_AVG_SIZE];
int movingAvgIndex = 0;

// Status Sistem
DHT dht(DHTPIN, DHTTYPE);
bool isFanOn = false;
bool isPumpOn = false;
bool isPumpLocked = false;
volatile bool isEstopActive = false; // Flag Interrupt E-Stop

unsigned long lastSensorRead = 0;
unsigned long lastMqttRetry  = 0;
unsigned long pumpStartTime  = 0;
unsigned long pumpLockoutTime = 0;

// Prototipe Fungsi
void setFan(bool state);
void setPump(bool state);
void emergencyShutdown();
void IRAM_ATTR estopISR();
int getMedianFilteredADC(int rawAdc);
float getMovingAverageADC(float medianAdc);
float convertADCToMoisture(float adcFiltered);
void handleNetworkNonBlocking();
void processControlLogic(float temp, float hum, float moisture, bool dhtFault, bool soilFault);
void publishTelemetry(float temp, float hum, float moisture, bool dhtFault, bool soilFault);

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Inisialisasi Hardware Pin
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(ESTOP_PIN, INPUT_PULLUP);

  // Pastikan aktuator mati saat booting awal
  setFan(false);
  setPump(false);

  // Attach Interrupt Hardware untuk Emergency Stop
  attachInterrupt(digitalPinToInterrupt(ESTOP_PIN), estopISR, CHANGE);

  // Inisialisasi Buffer Filter
  for (int i = 0; i < MEDIAN_SIZE; i++) medianBuffer[i] = 0;
  for (int i = 0; i < MOVING_AVG_SIZE; i++) movingAvgBuffer[i] = 0.0;

  dht.begin();
  
  // Konfigurasi Network
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("==================================================");
  Serial.println("   SMART GREENHOUSE FIRMWARE - FAIL-SAFE READY    ");
  Serial.println("==================================================");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  // 1. CEK HARDWARE EMERGENCY STOP (PRIORITAS UTAMA)
  if (digitalRead(ESTOP_PIN) == LOW || isEstopActive) {
    emergencyShutdown();
    return; // Stop seluruh eksekusi logika lainnya
  }

  // 2. KONEKTIVITAS NETWORK FAIL-SAFE (NON-BLOCKING)
  // Tidak menggunakan while() agar kontrol lokal tetap berjalan tanpa internet
  handleNetworkNonBlocking();

  // 3. PEMBACAAN & KONTROL LOKAL BERKALA (EVERY 1 SEC)
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;

    // Pembacaan DHT22
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    bool dhtFault = (isnan(temp) || isnan(hum));

    // Pembacaan Soil Moisture + Filter
    int adcRaw = analogRead(SOIL_PIN);
    int adcMedian = getMedianFilteredADC(adcRaw);
    float adcFiltered = getMovingAverageADC((float)adcMedian);
    bool soilFault = (adcFiltered >= ADC_AIR_THRESHOLD);
    float moisture = convertADCToMoisture(adcFiltered);

    // Proses Algoritma Kontrol Histeresis & Safety Fault Detection
    processControlLogic(temp, hum, moisture, dhtFault, soilFault);

    // Monitoring Serial
    Serial.print("DHT22: ");
    if (dhtFault) Serial.print("[FAULT]");
    else { Serial.print(temp, 1); Serial.print("C / "); Serial.print(hum, 1); Serial.print("%"); }

    Serial.print(" | Soil: ");
    if (soilFault) Serial.print("[AIR FAULT]");
    else { Serial.print(moisture, 1); Serial.print("%"); }

    Serial.print(" | Kipas: "); Serial.print(isFanOn ? "ON" : "OFF");
    Serial.print(" | Pompa: "); 
    if (isPumpLocked) Serial.println("LOCKED");
    else Serial.println(isPumpOn ? "ON" : "OFF");

    // Publish Telemetri ke MQTT Broker
    publishTelemetry(temp, hum, moisture, dhtFault, soilFault);
  }
}

// ==========================================
// IMPLEMENTASI FAIL-SAFE & KONTROL LOGIC
// ==========================================

/**
 * ISR Emergency Stop Switch
 */
void IRAM_ATTR estopISR() {
  if (digitalRead(ESTOP_PIN) == LOW) {
    isEstopActive = true;
  }
}

/**
 * Prosedur Matikan Seluruh Sistem dalam Kondisi Darurat
 */
void emergencyShutdown() {
  setFan(false);
  setPump(false);
  if (mqttClient.connected()) {
    mqttClient.publish("sg/sistem/mode", "EMERGENCY_STOP");
    mqttClient.publish("sg/aktuator/kipas", "OFF");
    mqttClient.publish("sg/aktuator/pompa", "OFF");
  }
  Serial.println("[EMERGENCY STOP] SYSTEM LOCKED! Tombol darurat aktif. Seluruh aktuator DIMATIKAN!");
  delay(500); // Debounce delay
}

/**
 * Network Handling Non-Blocking
 * Memastikan algoritma kontrol tetap berjalan walaupun WiFi/MQTT mati.
 */
void handleNetworkNonBlocking() {
  unsigned long currentMillis = millis();

  // Reconnect WiFi tanpa delay blocking
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastMqttRetry >= MQTT_RETRY_INT) {
      lastMqttRetry = currentMillis;
      WiFi.reconnect();
    }
    return;
  }

  // Reconnect MQTT tanpa delay blocking
  if (!mqttClient.connected()) {
    if (currentMillis - lastMqttRetry >= MQTT_RETRY_INT) {
      lastMqttRetry = currentMillis;
      if (mqttClient.connect("ESP32_Greenhouse_Client", "sg/sistem/koneksi", 1, true, "OFFLINE")) {
        Serial.println("[MQTT] Terhubung ke broker MQTT.");
        mqttClient.publish("sg/sistem/koneksi", "ONLINE", true);
        mqttClient.publish("sg/sistem/mode", "NORMAL", true);
        mqttClient.subscribe("sg/aktuator/#");
      }
    }
  } else {
    mqttClient.loop();
  }
}

/**
 * Pengiriman Data Telemetri ke MQTT Broker
 */
void publishTelemetry(float temp, float hum, float moisture, bool dhtFault, bool soilFault) {
  if (!mqttClient.connected()) return;

  char buf[12];
  if (!dhtFault) {
    dtostrf(temp, 1, 1, buf);
    mqttClient.publish("sg/sensor/suhu", buf);
    dtostrf(hum, 1, 1, buf);
    mqttClient.publish("sg/sensor/kelembaban_udara", buf);
  } else {
    mqttClient.publish("sg/sistem/error", "DHT22_FAULT");
  }

  if (!soilFault) {
    dtostrf(moisture, 1, 1, buf);
    mqttClient.publish("sg/sensor/kelembaban_tanah", buf);
  } else {
    mqttClient.publish("sg/sistem/error", "SOIL_SENSOR_AIR_FAULT");
  }

  mqttClient.publish("sg/aktuator/kipas", isFanOn ? "ON" : "OFF");
  mqttClient.publish("sg/aktuator/pompa", isPumpOn ? "ON" : "OFF");
}

/**
 * Algoritma Kontrol Histeresis + Fault Detection
 */
void processControlLogic(float temp, float hum, float moisture, bool dhtFault, bool soilFault) {
  unsigned long currentMillis = millis();

  // --- KONTROL SUHU (DHT22) ---
  if (dhtFault) {
    // FAULT DETECTION: Sensor DHT22 rusak/tercabut -> Matikan Kipas
    if (isFanOn) {
      setFan(false);
      Serial.println("[FAULT] Sensor DHT22 error/tercabut. Kipas dimatikan demi keamanan!");
    }
  } else {
    // Histeresis Kipas
    if (!isFanOn && temp > TEMP_FAN_ON) {
      setFan(true);
    } else if (isFanOn && temp <= TEMP_FAN_OFF) {
      setFan(false);
    }
  }

  // --- KONTROL IRIGASI (SOIL MOISTURE) ---
  if (soilFault) {
    // FAULT DETECTION: Sensor tanah Tercabut/Di Udara -> Matikan Pompa
    if (isPumpOn) {
      setPump(false);
      Serial.println("[FAULT] Sensor tanah terdeteksi di udara. Pompa dimatikan!");
    }
    return;
  }

  // Reset Lockout/Cooldown Pompa
  if (isPumpLocked) {
    if (currentMillis - pumpLockoutTime >= PUMP_COOLDOWN) {
      isPumpLocked = false;
    } else {
      return; // Tetap kunci pompa selama masa cooldown
    }
  }

  // Histeresis Pompa
  if (!isPumpOn && moisture < MOISTURE_PUMP_ON) {
    setPump(true);
    pumpStartTime = currentMillis;
  } else if (isPumpOn && moisture >= MOISTURE_PUMP_OFF) {
    setPump(false);
  }

  // Timeout Protection Pompa Kontinu (Max 10 Detik)
  if (isPumpOn) {
    if (currentMillis - pumpStartTime >= MAX_PUMP_ON_TIME) {
      setPump(false);
      isPumpLocked = true;
      pumpLockoutTime = currentMillis;
      Serial.println("[SAFETY] Pompa menyala > 10 detik kontinu! Pump Timeout & Cooldown Aktif.");
    }
  }
}

// ==========================================
// FUNGSI HELPER AKTUATOR & FILTER
// ==========================================

void setFan(bool turnOn) {
  isFanOn = turnOn;
  digitalWrite(FAN_PIN, ACTUATOR_ACTIVE_LOW ? (turnOn ? LOW : HIGH) : (turnOn ? HIGH : LOW));
}

void setPump(bool turnOn) {
  isPumpOn = turnOn;
  digitalWrite(PUMP_PIN, ACTUATOR_ACTIVE_LOW ? (turnOn ? LOW : HIGH) : (turnOn ? HIGH : LOW));
}

int getMedianFilteredADC(int rawAdc) {
  medianBuffer[medianIndex] = rawAdc;
  medianIndex = (medianIndex + 1) % MEDIAN_SIZE;
  int temp[MEDIAN_SIZE];
  for (int i = 0; i < MEDIAN_SIZE; i++) temp[i] = medianBuffer[i];
  for (int i = 0; i < MEDIAN_SIZE - 1; i++) {
    for (int j = 0; j < MEDIAN_SIZE - i - 1; j++) {
      if (temp[j] > temp[j + 1]) {
        int swapVal = temp[j]; temp[j] = temp[j + 1]; temp[j + 1] = swapVal;
      }
    }
  }
  return temp[MEDIAN_SIZE / 2];
}

float getMovingAverageADC(float medianAdc) {
  movingAvgBuffer[movingAvgIndex] = medianAdc;
  movingAvgIndex = (movingAvgIndex + 1) % MOVING_AVG_SIZE;
  float sum = 0;
  for (int i = 0; i < MOVING_AVG_SIZE; i++) sum += movingAvgBuffer[i];
  return sum / MOVING_AVG_SIZE;
}

float convertADCToMoisture(float adcFiltered) {
  float span = ADC_DRY - ADC_WET;
  float moisture = ((ADC_DRY - adcFiltered) / span) * 100.0;
  return constrain(moisture, 0.0, 100.0);
}
