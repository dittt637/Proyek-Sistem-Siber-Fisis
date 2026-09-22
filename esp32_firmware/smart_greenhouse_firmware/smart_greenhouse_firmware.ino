/*
 * Firmware Utama Smart Greenhouse (ESP32) - SCHEMATIC REVISI
 * Pembacaan Sensor & Kontrol Dual Closed-Loop + MQTT Telemetry
 * Dengan Pengandalan Auto-Reconnect WiFi & MQTT (Fail-Safe Jaringan)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// --- KONFIGURASI PIN GPIO (SCHEMATIC REVISI) ---
#define DHTPIN          34
#define DHTTYPE         DHT22
#define SOIL_PIN        4
#define TR_PUMP_PIN     27
#define TR_FAN_PIN      26

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
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
}

// Fungsi Reconnect WiFi jika Hotspot/WiFi mendadak mati
void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi Terputus! Memulai ulang sambungan WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Berhasil Terhubung Kembali! IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("\nWiFi belum siap, mencoba lagi...");
    }
  }
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

void reconnectMQTT() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Mencoba koneksi ke MQTT Broker...");
    String clientId = "ESP32-Greenhouse-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), "sg/sistem/koneksi", 1, true, "OFFLINE")) {
      Serial.println(" Terhubung!");
      client.publish("sg/sistem/koneksi", "ONLINE", true);
      client.subscribe("sg/aktuator/#");
    } else {
      Serial.print(" Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 3 detik");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(TR_PUMP_PIN, OUTPUT);
  pinMode(TR_FAN_PIN, OUTPUT);
  
  digitalWrite(TR_PUMP_PIN, LOW);
  digitalWrite(TR_FAN_PIN, LOW);
  
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // 1. Pastikan WiFi terhubung dulu
  checkWiFi();

  // 2. Pastikan MQTT terhubung
  if (!client.connected()) {
    reconnectMQTT();
  } else {
    client.loop();
  }

  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    int rawSoil = analogRead(SOIL_PIN);
    float soilMoisturePercent = map(rawSoil, 4095, 1500, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);

    // KONTROL LOCAL TETAP BERJALAN MESKIPUN INTERNET TERPUTUS (EDGE COMPUTING)
    if (!isnan(temp)) {
      if (temp > 35.0) {
        digitalWrite(TR_FAN_PIN, HIGH);
        if (client.connected()) client.publish("sg/aktuator/kipas", "ON");
      } else if (temp <= 28.0) {
        digitalWrite(TR_FAN_PIN, LOW);
        if (client.connected()) client.publish("sg/aktuator/kipas", "OFF");
      }
    }

    if (soilMoisturePercent < 30.0) {
      digitalWrite(TR_PUMP_PIN, HIGH);
      if (client.connected()) client.publish("sg/aktuator/pompa", "ON");
    } else if (soilMoisturePercent >= 60.0) {
      digitalWrite(TR_PUMP_PIN, LOW);
      if (client.connected()) client.publish("sg/aktuator/pompa", "OFF");
    }

    // PUBLISH DATA SENSOR JIKA MQTT TERHUBUNG
    if (client.connected() && !isnan(temp) && !isnan(hum)) {
      char strBuffer[10];
      
      dtostrf(temp, 1, 2, strBuffer);
      client.publish("sg/sensor/suhu", strBuffer);
      
      dtostrf(hum, 1, 2, strBuffer);
      client.publish("sg/sensor/kelembaban_udara", strBuffer);
      
      dtostrf(soilMoisturePercent, 1, 2, strBuffer);
      client.publish("sg/sensor/kelembaban_tanah", strBuffer);
    }
  }
}
