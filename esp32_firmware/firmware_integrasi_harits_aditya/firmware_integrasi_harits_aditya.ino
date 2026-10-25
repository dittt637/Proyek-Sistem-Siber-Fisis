/*
 * ============================================================================
 * FIRMWARE SMART GREENHOUSE ESP32 - INTEGRASI EMBEDDED & NETWORK IoT
 * Kolaborasi : Harits (Embedded & Control) + Aditya Rahman (Network & IoT)
 * Hardware   : DHT22 (Pin 4), Soil Moisture (Pin 34), Kipas (Pin 27), Pompa (Pin 26)
 * Keamanan   : E-Stop Physical Switch (Pin 14) - Active LOW Interrupt
 * Protokol   : MQTT via Cloud Public Broker (broker.emqx.io:1883)
 * Kontrak    : SPEC.md Versi 2.1.0 (Full JSON Payload + LWT + Alert)
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "DHT.h"

// ==========================================
// 1. PIN DEFINITION & HARDWARE CONFIG (HARITS)
// ==========================================
#define DHTPIN        4     // Pin Data DHT22
#define DHTTYPE       DHT22 // Tipe Sensor
#define SOIL_PIN      34    // Pin Analog Soil Sensor (ADC1)
#define FAN_PIN       27    // Pin Relay/Driver Kipas
#define PUMP_PIN      26    // Pin Relay/Driver Pompa
#define ESTOP_PIN     14    // Pin Emergency Stop Switch (INPUT_PULLUP)

const bool ACTUATOR_ACTIVE_LOW = true; // Set TRUE jika modul Relay Active-LOW

// ==========================================
// 2. KALIBRASI & HISTERESIS KONTROL (HARITS)
// ==========================================
const float ADC_DRY           = 3298.0; // 0% Moisture
const float ADC_WET           = 1193.0; // 100% Moisture
const float ADC_AIR_THRESHOLD = 3380.0; // Threshold Sensor Tercabut/Di Udara

// Ambang Batas Histeresis
const float TEMP_FAN_ON       = 35.0; // Kipas ON > 35.0 C
const float TEMP_FAN_OFF      = 28.0; // Kipas OFF <= 28.0 C
const float MOISTURE_PUMP_ON  = 30.0; // Pompa ON < 30.0%
const float MOISTURE_PUMP_OFF = 60.0; // Pompa OFF >= 60.0%

// Batas Pengaman (Safety & Timing)
const unsigned long MAX_PUMP_ON_TIME = 10000; // Maksimal pompa nyala kontinu 10 detik
const unsigned long PUMP_COOLDOWN    = 15000; // Cooldown istirahat pompa 15 detik
const unsigned long SENSOR_INTERVAL  = 2000;  // Interval publish sensor (2 detik sesuai SPEC.md)
const unsigned long MQTT_RETRY_INT   = 5000;  // Interval percobaan reconnect MQTT (5 detik)

// ==========================================
// 3. NETWORK & MQTT CONFIGURATION (ADITYA)
// ==========================================
// GANTI DENGAN NAMA HOTSPOT / WIFI ANDA:
const char* WIFI_SSID     = "NAMA_HOTSPOT_ANDA"; 
const char* WIFI_PASSWORD = "PASSWORD_HOTSPOT";  

// Cloud Public Broker (Menggantikan IP Lokal 192.168.1.100 Harits)
const char* MQTT_SERVER   = "broker.emqx.io";
const int   MQTT_PORT     = 1883;

// Daftar Topik Resmi SPEC.md
const char* TOPIC_SENSOR_DATA     = "greenhouse/sensor/data";
const char* TOPIC_ACTUATOR_STATUS = "greenhouse/actuator/status";
const char* TOPIC_SYSTEM_STATUS   = "greenhouse/system/status";
const char* TOPIC_ALERT           = "greenhouse/alert";
const char* TOPIC_ACTUATOR_CMD    = "greenhouse/actuator/command"; // Downlink kontrol dua arah

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==========================================
// 4. STRUKTUR DATA FILTER SENSOR (HARITS)
// ==========================================
#define MEDIAN_SIZE 15
#define MOVING_AVG_SIZE 5

int medianBuffer[MEDIAN_SIZE];
int medianIndex = 0;
float movingAvgBuffer[MOVING_AVG_SIZE];
int movingAvgIndex = 0;

DHT dht(DHTPIN, DHTTYPE);

// Variabel Status Sistem
bool isFanOn          = false;
bool isPumpOn         = false;
bool isPumpLocked     = false;
volatile bool isEstopActive = false; // Flag Interrupt E-Stop

bool prevFanState     = false;
bool prevPumpState    = false;
bool prevEstopState   = false;

unsigned long seqNumber        = 0; // Sequence message counter
unsigned long lastSensorRead   = 0;
unsigned long lastMqttRetry    = 0;
unsigned long pumpStartTime    = 0;
unsigned long pumpLockoutTime  = 0;

// Prototipe Fungsi
void setFan(bool state);
void setPump(bool state);
void emergencyShutdown();
void IRAM_ATTR estopISR();
int getMedianFilteredADC(int rawAdc);
float getMovingAverageADC(float medianAdc);
float convertADCToMoisture(float adcFiltered);
void handleNetworkNonBlocking();
void mqttCallback(char* topic, byte* payload, unsigned int length);
time_t getEpochTime();
void publishSensorData(float temp, float hum, int soil, bool dhtFault);
void publishActuatorStatus();
void publishSystemStatus(bool online, bool sensorFault);
void publishAlert(const char* level, const char* code, const char* message);
void processControlLogic(float temp, float hum, float moisture, bool dhtFault, bool soilFault);

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

  // Inisialisasi Buffer Filter Sensor
  for (int i = 0; i < MEDIAN_SIZE; i++) medianBuffer[i] = 0;
  for (int i = 0; i < MOVING_AVG_SIZE; i++) movingAvgBuffer[i] = 0.0;

  dht.begin();

  // Inisialisasi Koneksi Jaringan (Non-blocking STA)
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Konfigurasi Client MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // Sinkronisasi Waktu NTP (WIB UTC+7) untuk Timestamp SPEC.md
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.println("\n==================================================");
  Serial.println("  SMART GREENHOUSE - INTEGRASI EMBEDDED + MQTT    ");
  Serial.println("==================================================");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  // 1. CEK HARDWARE EMERGENCY STOP (PRIORITAS UTAMA)
  if (digitalRead(ESTOP_PIN) == LOW || isEstopActive) {
    emergencyShutdown();
    return; // Hentikan eksekusi logika lainnya selama E-Stop aktif
  }

  // 2. KONEKSI NETWORK FAIL-SAFE (NON-BLOCKING)
  handleNetworkNonBlocking();

  // 3. PEMBACAAN SENSOR & KONTROL LOKAL (SETIAP 2 DETIK)
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;

    // Pembacaan DHT22
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    bool dhtFault = (isnan(temp) || isnan(hum));

    // Pembacaan Soil Moisture + Filter Kombinasi Median & Moving Average
    int adcRaw = analogRead(SOIL_PIN);
    int adcMedian = getMedianFilteredADC(adcRaw);
    float adcFiltered = getMovingAverageADC((float)adcMedian);
    bool soilFault = (adcFiltered >= ADC_AIR_THRESHOLD);
    float moisture = convertADCToMoisture(adcFiltered);
    int soilInt = (int)constrain(moisture, 0.0, 100.0);

    // Proses Algoritma Histeresis & Pengaman Pompa
    processControlLogic(temp, hum, moisture, dhtFault, soilFault);

    // Kirim Data Telemetri JSON ke MQTT Broker
    publishSensorData(temp, hum, soilInt, dhtFault);

    // Kirim Update Status Aktuator jika terjadi perubahan kondisi fisik
    if (isFanOn != prevFanState || isPumpOn != prevPumpState) {
      publishActuatorStatus();
      prevFanState = isFanOn;
      prevPumpState = isPumpOn;
    }

    // Monitoring Log Serial
    Serial.printf("[SENSOR] Temp: %.1f C | Hum: %.1f %% | Soil: %d %% | Seq: %lu | Kipas: %s | Pompa: %s\n",
                  dhtFault ? 0.0 : temp, dhtFault ? 0.0 : hum, soilInt, seqNumber,
                  isFanOn ? "ON" : "OFF", isPumpOn ? "ON" : "OFF");
  }
}

// ==========================================
// PENGELOLAAN NETWORK & MQTT (ADITYA)
// ==========================================

void handleNetworkNonBlocking() {
  unsigned long currentMillis = millis();

  // Reconnect WiFi jika terputus (tanpa blocking loop)
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastMqttRetry >= MQTT_RETRY_INT) {
      lastMqttRetry = currentMillis;
      WiFi.reconnect();
    }
    return;
  }

  // Reconnect MQTT jika terputus
  if (!mqttClient.connected()) {
    if (currentMillis - lastMqttRetry >= MQTT_RETRY_INT) {
      lastMqttRetry = currentMillis;

      // Last Will & Testament (LWT) sesuai SPEC.md Topic 3
      const char* lwtPayload = "{\"state\":\"offline\",\"sensor_fault\":false}";
      String clientId = "ESP32_Greenhouse_" + String(random(0xffff), HEX);

      if (mqttClient.connect(clientId.c_str(), "", "", TOPIC_SYSTEM_STATUS, 1, true, lwtPayload)) {
        Serial.println("[MQTT] Terhubung ke Cloud Broker EMQX!");

        // Berlangganan topik perintah kontrol dua arah
        mqttClient.subscribe(TOPIC_ACTUATOR_CMD);

        // Publish status awal perangkat saat tersambung
        publishSystemStatus(true, false);
        publishActuatorStatus();
      }
    }
  } else {
    mqttClient.loop();
  }
}

/**
 * Callback Pesan Masuk (Komunikasi Dua Arah / Downlink Control)
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("[MQTT RECV] Topik: %s | Pesan: %s\n", topic, message.c_str());

  // Parser perintah sederhana untuk kontrol manual dari Dashboard / MQTTX
  if (String(topic) == TOPIC_ACTUATOR_CMD) {
    if (message.indexOf("\"fan\":\"ON\"") >= 0 || message.indexOf("\"action\":\"FAN_ON\"") >= 0) {
      setFan(true);
      publishActuatorStatus();
    } else if (message.indexOf("\"fan\":\"OFF\"") >= 0 || message.indexOf("\"action\":\"FAN_OFF\"") >= 0) {
      setFan(false);
      publishActuatorStatus();
    } else if (message.indexOf("\"pump\":\"ON\"") >= 0 || message.indexOf("\"action\":\"PUMP_ON\"") >= 0) {
      if (!isPumpLocked) {
        setPump(true);
        pumpStartTime = millis();
        publishActuatorStatus();
      }
    } else if (message.indexOf("\"pump\":\"OFF\"") >= 0 || message.indexOf("\"action\":\"PUMP_OFF\"") >= 0) {
      setPump(false);
      publishActuatorStatus();
    }
  }
}

time_t getEpochTime() {
  time_t now;
  time(&now);
  return now;
}

/**
 * Topic 1: greenhouse/sensor/data (QoS 1, Retain: false)
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
 * Topic 2: greenhouse/actuator/status (QoS 1, Retain: true)
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
 * Topic 3: greenhouse/system/status (QoS 1, Retain: true)
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
 * Topic 4: greenhouse/alert (QoS 1, Retain: false)
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
// LOGIKA KONTROL & EMERGENCY STOP (HARITS)
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
    publishAlert("critical", "ESTOP", "Tombol Emergency Stop aktif! Seluruh aktuator diputus.");
    publishActuatorStatus();
  }
  
  Serial.println("[EMERGENCY STOP] SISTEM TERKUNCI! Tombol darurat aktif.");
  delay(500); // Debounce delay
}

void processControlLogic(float temp, float hum, float moisture, bool dhtFault, bool soilFault) {
  unsigned long currentMillis = millis();

  // --- KONTROL SUHU (KIPAS) ---
  if (dhtFault) {
    if (isFanOn) {
      setFan(false);
      Serial.println("[FAULT] Sensor DHT22 rusak/tercabut. Kipas dimatikan.");
      publishAlert("critical", "SENSOR_FAULT", "Sensor DHT22 rusak, kipas dimatikan.");
    }
  } else {
    if (!isFanOn && temp > TEMP_FAN_ON) {
      setFan(true);
      publishAlert("critical", "TEMP_CRITICAL", "Suhu udara melebihi batas 35 C, kipas menyala.");
    } else if (isFanOn && temp <= TEMP_FAN_OFF) {
      setFan(false);
    }
  }

  // --- KONTROL IRIGASI (POMPA) ---
  if (soilFault) {
    if (isPumpOn) {
      setPump(false);
      Serial.println("[FAULT] Sensor tanah terdeteksi di udara. Pompa dimatikan.");
      publishAlert("critical", "SENSOR_FAULT", "Sensor tanah terlepas, pompa dimatikan.");
    }
    return;
  }

  // Reset Lockout Pompa (Cooldown 15 Detik)
  if (isPumpLocked) {
    if (currentMillis - pumpLockoutTime >= PUMP_COOLDOWN) {
      isPumpLocked = false;
      Serial.println("[SAFETY] Cooldown selesai. Pompa siap kembali.");
    } else {
      return; // Pompa terkunci selama masa cooldown
    }
  }

  // Histeresis Pompa
  if (!isPumpOn && moisture < MOISTURE_PUMP_ON) {
    setPump(true);
    pumpStartTime = currentMillis;
    publishAlert("warning", "SOIL_DRY", "Kelembaban tanah < 30%, pompa menyala.");
  } else if (isPumpOn && moisture >= MOISTURE_PUMP_OFF) {
    setPump(false);
  }

  // Timeout Protection Pompa Kontinu (Max 10 Detik)
  if (isPumpOn) {
    if (currentMillis - pumpStartTime >= MAX_PUMP_ON_TIME) {
      setPump(false);
      isPumpLocked = true;
      pumpLockoutTime = currentMillis;
      Serial.println("[SAFETY] Pompa menyala > 10 detik kontinu! Timeout & Cooldown Aktif.");
    }
  }
}

// ==========================================
// FUNGSI HELPER AKTUATOR & FILTER SENSOR (HARITS)
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
