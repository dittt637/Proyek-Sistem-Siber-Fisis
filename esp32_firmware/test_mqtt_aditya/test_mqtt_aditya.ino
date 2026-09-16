#include <WiFi.h>
#include <PubSubClient.h>

// --- KONFIGURASI WIFI ---
const char* ssid = "NAMA_WIFI_ANDA";       // Ganti dengan nama WiFi Anda
const char* password = "PASSWORD_WIFI";    // Ganti dengan password WiFi Anda

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

void reconnect() {
  // Looping sampai terhubung kembali
  while (!client.connected()) {
    Serial.print("Mencoba koneksi MQTT...");
    // Membuat Client ID acak
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Mencoba terhubung (Gunakan LWT disini nantinya untuk Fail-Safe)
    if (client.connect(clientId.c_str())) {
      Serial.println("Terhubung!");
      // Setelah terhubung, langsung subscribe ke topik aktuator
      client.subscribe("sg/aktuator/#");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik");
      delay(5000);
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
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  // Mengirim data simulasi setiap 5 detik
  if (now - lastMsg > 5000) {
    lastMsg = now;
    
    // Simulasi nilai suhu
    float suhu_dummy = random(250, 380) / 10.0; 
    char suhuString[8];
    dtostrf(suhu_dummy, 1, 2, suhuString); // Konversi float ke string
    
    Serial.print("Publish pesan: ");
    Serial.println(suhuString);
    
    // Publish ke topik MQTT
    client.publish("sg/sensor/suhu", suhuString);
  }
}
