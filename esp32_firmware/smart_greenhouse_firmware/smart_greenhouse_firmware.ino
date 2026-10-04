/*
 * ============================================================================
 * FIRMWARE SMART GREENHOUSE ESP32 - FULL SPEC.md (OPSI 1) & FAIL-SAFE ARCHITECTURE
 * Mengimplementasikan Kontrak Data MQTT Sesuai SPEC.md Versi 2.1.0
 * 
 * Hardware Config:
 * - DHT22 Data       : GPIO 4
 * - Soil Moisture ADC: GPIO 34
 * - Kipas 12V        : GPIO 27
 * - Pompa 5V         : GPIO 26
 * - Emergency Stop   : GPIO 14 (Interrupt INPUT_PULLUP)
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "DHT.h"

// ==========================================
// 1. PIN DEFINITION & HARDWARE CONFIG
// ==========================================
#define DHTPIN        4     // Pin Data DHT22
#define DHTTYPE       DHT22 // Tipe Sensor
#define SOIL_PIN      34    // Pin Analog Soil Sensor (ADC1)
#define FAN_PIN       27    // Pin Relay/Driver Kipas
#define PUMP_PIN      26    // Pin Relay/Driver Pompa
#define ESTOP_PIN     14    // Pin Emergency Stop Switch (INPUT_PULLUP)

const bool ACTUATOR_ACTIVE_LOW = true; // Set TRUE jika Active LOW (Relay Module), FALSE jika Transistor

// ==========================================
// 2. KONSTANTA KALIBRASI & HISTERESIS (SPEC.md)
// ==========================================
const float ADC_DRY           = 3298.0; // 0% Moisture
const float ADC_WET           = 1193.0; // 100% Moisture
const float ADC_AIR_THRESHOLD = 3380.0; // Sensor tercabut/di udara

// Histeresis Kontrol
const float TEMP_FAN_ON       = 35.0; // Kipas ON > 35.0 C
const float TEMP_FAN_OFF      = 28.0; // Kipas OFF <= 28.0 C
const float MOISTURE_PUMP_ON  = 30.0; // Pompa ON < 30.0%
const float MOISTURE_PUMP_OFF = 60.0; // Pompa OFF >= 60.0%

// Safety & Timing (SPEC.md)
const unsigned long MAX_PUMP_ON_TIME = 10000; // Maksimal pompa nyala 10 detik
const unsigned long PUMP_COOLDOWN    = 15000; // Jeda istirahat pompa 15 detik
const unsigned long SENSOR_INTERVAL  = 2000;  // Interval publish sensor = 2 detik (SPEC.md A2)
const unsigned long MQTT_RETRY_INT   = 5000;  // Interval reconnect MQTT (5s)

// ==========================================
// 3. NETWORK CONFIGURATION & MQTT TOPICS
// ==========================================
const char* WIFI_SSID     = "MONITORING-ESP32"; // Ganti dengan nama WiFi/Hotspot Anda
const char* WIFI_PASSWORD = "esp32monitor";     // Ganti dengan password WiFi Anda
const char* MQTT_SERVER   = "broker.emqx.io";   // Cloud Public Broker
const int   MQTT_PORT     = 1883;

// Topic Resmi SPEC.md
const char* TOPIC_SENSOR_DATA     = "greenhouse/sensor/data";
const char* TOPIC_ACTUATOR_STATUS = "greenhouse/actuator/status";
const char* TOPIC_SYSTEM_STATUS   = "greenhouse/system/status";
const char* TOPIC_ALERT           = "greenhouse/alert";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==========================================
// 4. STRUKTUR DATA FILTER & VARIABEL GLOBAL
// ==========================================
#define MEDIAN_SIZE 15
#define MOVING_AVG_SIZE 5

int medianBuffer[MEDIAN_SIZE];
int medianIndex = 0;
float movingAvgBuffer[MOVING_AVG_SIZE];
int movingAvgIndex = 0;

DHT dht(DHTPIN, DHTTYPE);

bool isFanOn = false;
bool isPumpOn = false;
bool isPumpLocked = false;
volatile bool isEstopActive = false;
bool prevFanState = false;
bool prevPumpState = false;
bool prevEstopState = false;

unsigned long seqNumber       = 0; // Sequence message counter untuk evaluasi packet loss
unsigned long lastSensorRead  = 0;
unsigned long lastMqttRetry   = 0;
unsigned long pumpStartTime   = 0;
unsigned long pumpLockoutTime = 0;

// Prototipe Fungsi
void setup_wifi();
void handleNetworkNonBlocking();
time_t getEpochTime();
void publishSensorData(float temp, float hum, int soil, bool dhtFault);
void publishActuatorStatus();
void publishSystemStatus(bool online, bool sensorFault);
void publishAlert(const char* level, const char* code, const char* message);
void setFan(bool state);
void setPump(bool state);
void emergencyShutdown();
void IRAM_ATTR estopISR();
int getMedianFilteredADC(int rawAdc);
float getMovingAverageADC(float medianAdc);
float convertADCToMoisture(float adcFiltered);
void processControlLogic(float temp, float hum, float moisture, bool dhtFault, bool soilFault);

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(ESTOP_PIN, INPUT_PULLUP);

  setFan(false);
  setPump(false);

  attachInterrupt(digitalPinToInterrupt(ESTOP_PIN), estopISR, CHANGE);

  for (int i = 0; i < MEDIAN_SIZE; i++) medianBuffer[i] = 0;
  for (int i = 0; i < MOVING_AVG_SIZE; i++) movingAvgBuffer[i] = 0.0;

  dht.begin();
  setup_wifi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("==================================================");
  Serial.println("   SMART GREENHOUSE - OPSI 1 SPEC.md JSON READY   ");
  Serial.println("==================================================");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  // 1. CEK HARDWARE EMERGENCY STOP (PRIORITAS 1)
  if (digitalRead(ESTOP_PIN) == LOW || isEstopActive) {
    emergencyShutdown();
    return;
  }

  // 2. KONEKSI NETWORK NON-BLOCKING (FAIL-SAFE)
  handleNetworkNonBlocking();

  // 3. PEMBACAAN DAN EKSEKUSI KONTROL SETIAP 2 DETIK (SPEC.md Interval)
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
    int soilInt = (int)constrain(moisture, 0.0, 100.0);

    // Proses Algoritma Dual Closed-Loop & Safety
    processControlLogic(temp, hum, moisture, dhtFault, soilFault);

    // Publish Sensor JSON Sesuai SPEC.md
    publishSensorData(temp, hum, soilInt, dhtFault);

    // Cek apakah status aktuator berubah, jika berubah publish update retained
    if (isFanOn != prevFanState || isPumpOn != prevPumpState) {
      publishActuatorStatus();
      prevFanState = isFanOn;
      prevPumpState = isPumpOn;
    }

    // Monitoring Serial
    Serial.printf("[SENSOR] Temp: %.1f C | Hum: %.1f %% | Soil: %d %% | Seq: %lu | Kipas: %s | Pompa: %s\n",
                  dhtFault ? 0.0 : temp, dhtFault ? 0.0 : hum, soilInt, seqNumber,
                  isFanOn ? "ON" : "OFF", isPumpOn ? "ON" : "OFF");
  }
}

// ==========================================
// IMPLEMENTASI NETWORK & SPEC.md MQTT JSON
// ==========================================

void setup_wifi() {
  delay(10);
  Serial.print("\nConnecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());

  // Sinkronisasi Waktu NTP (UTC+7 WIB untuk SPEC.md Timestamp)
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("[NTP] Sinkronisasi waktu dimulai...");
}

time_t getEpochTime() {
  time_t now;
  time(&now);
  return now;
}

void handleNetworkNonBlocking() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastMqttRetry >= MQTT_RETRY_INT) {
      lastMqttRetry = currentMillis;
      WiFi.reconnect();
    }
    return;
  }

  if (!mqttClient.connected()) {
    if (currentMillis - lastMqttRetry >= MQTT_RETRY_INT) {
      lastMqttRetry = currentMillis;

      // LWT (Last Will and Testament) sesuai SPEC.md Topic 3
      const char* lwtPayload = "{\"state\":\"offline\",\"sensor_fault\":false}";
      String clientId = "ESP32_Greenhouse_" + String(random(0xffff), HEX);

      if (mqttClient.connect(clientId.c_str(), "", "", TOPIC_SYSTEM_STATUS, 1, true, lwtPayload)) {
        Serial.println("[MQTT] Terhubung ke Broker EMQX!");
        
        // Publish Status Awal saat terhubung (Retained: true, QoS 1)
        publishSystemStatus(true, false);
        publishActuatorStatus();
      }
    }
  } else {
    mqttClient.loop();
  }
}

/**
 * Topic 1: greenhouse/sensor/data
 * Payload JSON: {"temp": 31.5, "hum": 68.2, "soil": 42, "seq": 1024, "ts": 1759500000}
 */
void publishSensorData(float temp, float hum, int soil, bool dhtFault) {
  if (!mqttClient.connected()) return;

  seqNumber++;
  time_t ts = getEpochTime();
  char payload[160];

  if (dhtFault) {
    snprintf(payload, sizeof(payload),
             "{\"temp\":null,\"hum\":null,\"soil\":%d,\"seq\":%lu,\"ts\":%lu}",
             soil, seqNumber, (unsigned long)ts);
    publishSystemStatus(true, true);
    publishAlert("critical", "SENSOR_FAULT", "Sensor DHT22 gagal membaca data");
  } else {
    snprintf(payload, sizeof(payload),
             "{\"temp\":%.1f,\"hum\":%.1f,\"soil\":%d,\"seq\":%lu,\"ts\":%lu}",
             temp, hum, soil, seqNumber, (unsigned long)ts);
  }

  mqttClient.publish(TOPIC_SENSOR_DATA, payload, false);
}

/**
 * Topic 2: greenhouse/actuator/status (Retained: true, QoS 1)
 * Payload JSON: {"fan": true, "pump": false, "mode": "auto", "estop": false, "ts": 1759500000}
 */
void publishActuatorStatus() {
  if (!mqttClient.connected()) return;

  time_t ts = getEpochTime();
  char payload[160];
  const char* modeStr = isEstopActive ? "emergency" : (WiFi.status() == WL_CONNECTED ? "auto" : "offline");

  snprintf(payload, sizeof(payload),
           "{\"fan\":%s,\"pump\":%s,\"mode\":\"%s\",\"estop\":%s,\"ts\":%lu}",
           isFanOn ? "true" : "false",
           isPumpOn ? "true" : "false",
           modeStr,
           isEstopActive ? "true" : "false",
           (unsigned long)ts);

  mqttClient.publish(TOPIC_ACTUATOR_STATUS, payload, true);
}

/**
 * Topic 3: greenhouse/system/status (Retained: true, QoS 1)
 * Payload JSON: {"state": "online", "sensor_fault": false}
 */
void publishSystemStatus(bool online, bool sensorFault) {
  if (!mqttClient.connected()) return;

  char payload[80];
  snprintf(payload, sizeof(payload),
           "{\"state\":\"%s\",\"sensor_fault\":%s}",
           online ? "online" : "offline",
           sensorFault ? "true" : "false");

  mqttClient.publish(TOPIC_SYSTEM_STATUS, payload, true);
}

/**
 * Topic 4: greenhouse/alert (Retained: false, QoS 1)
 * Payload JSON: {"level": "warning", "code": "TEMP_CRITICAL", "message": "...", "ts": 1759500000}
 */
void publishAlert(const char* level, const char* code, const char* message) {
  if (!mqttClient.connected()) return;

  time_t ts = getEpochTime();
  char payload[200];
  snprintf(payload, sizeof(payload),
           "{\"level\":\"%s\",\"code\":\"%s\",\"message\":\"%s\",\"ts\":%lu}",
           level, code, message, (unsigned long)ts);

  mqttClient.publish(TOPIC_ALERT, payload, false);
}

// ==========================================
// KONTROL LOGIC & EMERGENCY
// ==========================================

void IRAM_ATTR estopISR() {
  if (digitalRead(ESTOP_PIN) == LOW) {
    isEstopActive = true;
  }
}

void emergencyShutdown() {
  setFan(false);
  setPump(false);
  if (!prevEstopState) {
    prevEstopState = true;
    publishAlert("critical", "ESTOP", "Tombol Emergency Stop ditekan! Seluruh aktuator dimatikan.");
    publishActuatorStatus();
  }
  Serial.println("[EMERGENCY STOP] SYSTEM LOCKED! Tombol darurat aktif.");
  delay(500);
}

void processControlLogic(float temp, float hum, float moisture, bool dhtFault, bool soilFault) {
  unsigned long currentMillis = millis();

  // 1. Kontrol Kipas (DHT22)
  if (dhtFault) {
    if (isFanOn) {
      setFan(false);
      publishAlert("critical", "SENSOR_FAULT", "Sensor suhu rusak, kipas dinonaktifkan otomatis.");
    }
  } else {
    if (!isFanOn && temp > TEMP_FAN_ON) {
      setFan(true);
      publishAlert("critical", "TEMP_CRITICAL", "Suhu udara melebihi batas kritis 35°C");
    } else if (isFanOn && temp <= TEMP_FAN_OFF) {
      setFan(false);
    }
  }

  // 2. Kontrol Pompa (Soil Moisture)
  if (soilFault) {
    if (isPumpOn) {
      setPump(false);
      publishAlert("critical", "SENSOR_FAULT", "Sensor tanah terlepas, pompa dimatikan.");
    }
    return;
  }

  // Reset Lockout Pompa
  if (isPumpLocked) {
    if (currentMillis - pumpLockoutTime >= PUMP_COOLDOWN) {
      isPumpLocked = false;
    } else {
      return;
    }
  }

  // Histeresis Pompa
  if (!isPumpOn && moisture < MOISTURE_PUMP_ON) {
    setPump(true);
    pumpStartTime = currentMillis;
    publishAlert("warning", "SOIL_DRY", "Kelembapan tanah rendah di bawah 30%, pompa menyala.");
  } else if (isPumpOn && moisture >= MOISTURE_PUMP_OFF) {
    setPump(false);
  }

  // Timeout Pompa Kontinu (Max 10 Detik)
  if (isPumpOn) {
    if (currentMillis - pumpStartTime >= MAX_PUMP_ON_TIME) {
      setPump(false);
      isPumpLocked = true;
      pumpLockoutTime = currentMillis;
      Serial.println("[SAFETY] Pompa menyala > 10 detik kontinu! Timeout aktif.");
    }
  }
}

// ==========================================
// HELPER HARDWARE & FILTERING
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
