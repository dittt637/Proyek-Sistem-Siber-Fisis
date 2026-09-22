/*
 * Firmware Utama Smart Greenhouse (ESP32)
 * Pembacaan Sensor & Kontrol Dual Closed-Loop + MQTT Telemetry
 * 
 * Pinout Mapping berdasarkan Wiring Diagram:
 * - DHT22 Data          : GPIO 4
 * - Soil Moisture Analog: GPIO 34
 * - Relay 1 (Pompa 5V)  : GPIO 27
 * - Relay 2 (Kipas 12V) : GPIO 26
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// --- KONFIGURASI PIN GPIO ---
#define DHTPIN          4
#define DHTTYPE         DHT22
#define SOIL_PIN        34
#define RELAY_PUMP_PIN  27
#define RELAY_FAN_PIN   26

// --- KONFIGURASI WIFI & MQTT ---
const char* ssid = "NAMA_WIFI_ANDA";
const char* password = "PASSWORD_WIFI";
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Pesan diterima di [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Mencoba koneksi ke MQTT Broker...");
    String clientId = "ESP32-Greenhouse-";
    clientId += String(random(0xffff), HEX);
    
    // Last Will & Testament (LWT) untuk Fail-Safe status koneksi
    if (client.connect(clientId.c_str(), "sg/sistem/koneksi", 1, true, "OFFLINE")) {
      Serial.println(" Terhubung!");
      // Notify System Status ONLINE
      client.publish("sg/sistem/koneksi", "ONLINE", true);
      client.subscribe("sg/aktuator/#");
    } else {
      Serial.print(" Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik.");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi Pin Relay (ACTIVE LOW / HIGH disesuaikan dengan modul relay)
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(RELAY_FAN_PIN, OUTPUT);
  
  // Matikan aktuator di awal (Default OFF)
  digitalWrite(RELAY_PUMP_PIN, HIGH);
  digitalWrite(RELAY_FAN_PIN, HIGH);
  
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  // Baca sensor & kontrol setiap 5 detik
  if (now - lastMsg > 5000) {
    lastMsg = now;

    // 1. Baca Sensor Suhu (DHT22)
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    // 2. Baca Sensor Kelembaban Tanah (ADC 0 - 4095)
    int rawSoil = analogRead(SOIL_PIN);
    // Konversi nilai ADC ke persentase (0% = kering, 100% = basah)
    float soilMoisturePercent = map(rawSoil, 4095, 1500, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);

    if (isnan(temp) || isnan(hum)) {
      Serial.println("Gagal membaca dari sensor DHT22!");
      client.publish("sg/sistem/error", "DHT22_READ_ERROR");
      return;
    }

    Serial.printf("Suhu: %.2f°C | Humidity: %.2f%% | Soil: %.2f%%\n", temp, hum, soilMoisturePercent);

    // --- DUAL CLOSED-LOOP CONTROL ---
    // A. Kontrol Kipas (Suhu > 35°C ON, <= 28°C OFF)
    if (temp > 35.0) {
      digitalWrite(RELAY_FAN_PIN, LOW); // Relay ON
      client.publish("sg/aktuator/kipas", "ON");
    } else if (temp <= 28.0) {
      digitalWrite(RELAY_FAN_PIN, HIGH); // Relay OFF
      client.publish("sg/aktuator/kipas", "OFF");
    }

    // B. Kontrol Pompa Air (Soil < 30% ON, >= 60% OFF)
    if (soilMoisturePercent < 30.0) {
      digitalWrite(RELAY_PUMP_PIN, LOW); // Relay ON
      client.publish("sg/aktuator/pompa", "ON");
    } else if (soilMoisturePercent >= 60.0) {
      digitalWrite(RELAY_PUMP_PIN, HIGH); // Relay OFF
      client.publish("sg/aktuator/pompa", "OFF");
    }

    // --- PUBLISH TELEMETRI KE MQTT ---
    char strBuffer[10];
    
    dtostrf(temp, 1, 2, strBuffer);
    client.publish("sg/sensor/suhu", strBuffer);
    
    dtostrf(hum, 1, 2, strBuffer);
    client.publish("sg/sensor/kelembaban_udara", strBuffer);
    
    dtostrf(soilMoisturePercent, 1, 2, strBuffer);
    client.publish("sg/sensor/kelembaban_tanah", strBuffer);
  }
}
