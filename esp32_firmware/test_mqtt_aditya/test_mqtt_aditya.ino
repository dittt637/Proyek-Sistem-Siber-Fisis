#include <WiFi.h>
#include <PubSubClient.h>

// --- KONFIGURASI WIFI ---
const char* ssid = "MONITORING-ESP32";       // Ganti dengan nama WiFi Anda
const char* password = "esp32monitor";    // Ganti dengan password WiFi Anda

// --- KONFIGURASI MQTT BROKER ---
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;

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

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Fungsi reconnect WiFi jika terputus
void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi terputus! Memulai ulang koneksi WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long startAttempt = millis();
    // Tunggu maksimal 10 detik untuk konek ulang ke WiFi
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Berhasil Terhubung Kembali! IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("\nWiFi masih belum siap, akan dicoba lagi...");
    }
  }
}

// Fungsi yang dijalankan saat ada pesan MQTT masuk (Subscribe)
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Pesan masuk di topik [");
  Serial.print(topic);
  Serial.print("] : ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnectMQTT() {
  // Hanya coba connect MQTT jika WiFi sudah dipastikan terhubung
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Mencoba koneksi MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Terhubung!");
      client.subscribe("sg/aktuator/#");
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
  client.setCallback(callback);
}

void loop() {
  // 1. Cek koneksi WiFi dulu
  checkWiFi();
  
  // 2. Cek koneksi MQTT
  if (!client.connected()) {
    reconnectMQTT();
  } else {
    client.loop();
  }

  unsigned long now = millis();
  // Mengirim data simulasi setiap 5 detik (hanya jika terhubung MQTT)
  if (client.connected() && (now - lastMsg > 5000)) {
    lastMsg = now;
    
    float suhu_dummy = random(250, 380) / 10.0; 
    char suhuString[8];
    dtostrf(suhu_dummy, 1, 2, suhuString);
    
    Serial.print("Publish pesan: ");
    Serial.println(suhuString);
    
    client.publish("sg/sensor/suhu", suhuString);
  }
}
