/*
 * Test Script ESP32 / ESP32-C3 untuk Aditya - OPSI 1 (SPEC.md JSON Payload)
 * Mensimulasikan pengiriman JSON ke Broker MQTT agar Dashboard Fathoni langsung terbaca!
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// --- KONFIGURASI WIFI & MQTT ---
const char* ssid = "MONITORING-ESP32";       // Ganti dengan nama WiFi / Hotspot Anda
const char* password = "esp32monitor";     // Ganti dengan password WiFi Anda
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
unsigned long seqCounter = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

  // Sinkronisasi Waktu NTP untuk Timestamp SPEC.md
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi terputus! Memulai ulang koneksi WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }
  }
}

void reconnectMQTT() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Mencoba koneksi MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    
    // LWT sesuai SPEC.md
    const char* lwtPayload = "{\"state\":\"offline\",\"sensor_fault\":false}";
    
    if (client.connect(clientId.c_str(), "", "", "greenhouse/system/status", 1, true, lwtPayload)) {
      Serial.println("Terhubung!");
      // Status online retained
      client.publish("greenhouse/system/status", "{\"state\":\"online\",\"sensor_fault\":false}", true);
      // Status aktuator retained awal
      client.publish("greenhouse/actuator/status", "{\"fan\":false,\"pump\":false,\"mode\":\"auto\",\"estop\":false,\"ts\":0}", true);
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 3 detik");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  checkWiFi();
  
  if (!client.connected()) {
    reconnectMQTT();
  } else {
    client.loop();
  }

  unsigned long now = millis();
  // Kirim data JSON setiap 2 detik (Interval resmi SPEC.md)
  if (client.connected() && (now - lastMsg > 2000)) {
    lastMsg = now;
    seqCounter++;
    
    time_t nowEpoch;
    time(&nowEpoch);
    
    // Nilai dummy acak realistis
    float temp = 28.0 + (random(0, 80) / 10.0); // 28.0 - 36.0 C
    float hum = 60.0 + (random(0, 150) / 10.0);  // 60.0 - 75.0 %
    int soil = random(35, 65);                   // 35 - 65 %
    
    // JSON Payload sesuai format SPEC.md Topic 1
    char payload[160];
    snprintf(payload, sizeof(payload),
             "{\"temp\":%.1f,\"hum\":%.1f,\"soil\":%d,\"seq\":%lu,\"ts\":%lu}",
             temp, hum, soil, seqCounter, (unsigned long)nowEpoch);
    
    Serial.print("Publish SPEC.md JSON: ");
    Serial.println(payload);
    
    // Publish ke greenhouse/sensor/data
    client.publish("greenhouse/sensor/data", payload);
  }
}
