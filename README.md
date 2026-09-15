# Sistem Pengendali Iklim dan Irigasi Smart Greenhouse

Proyek ini adalah implementasi *Cyber Physical System (CPS)* untuk pemantauan dan pengendalian lingkungan *greenhouse* secara otomatis. Sistem ini menggunakan protokol MQTT dan memiliki arsitektur *Fail-Safe*.

## Tim Proyek
1. **Bagus Satria Priambodo** - Hardware & Mechanical
2. **Fathoni Ibra A A** - Monitoring & Evaluation
3. **Muhammad Harits Arrosyid** - Embedded & Control
4. **Aditya Rahman** - Network & IoT

## Arsitektur MQTT (Draft Minggu 4)

Sistem ini menggunakan arsitektur Publish/Subscribe melalui MQTT Broker.

*   **Broker yang digunakan:** `broker.emqx.io` (Public Broker untuk fase testing)
*   **Port:** `1883`

### Struktur Topik MQTT

**1. Topik Telemetri (Data Sensor dari ESP32)**
*   `sg/sensor/suhu` (Payload: float, misal 32.5)
*   `sg/sensor/kelembaban_udara` (Payload: float, misal 60.0)
*   `sg/sensor/kelembaban_tanah` (Payload: float, misal 45)

**2. Topik Status Aktuator**
*   `sg/aktuator/kipas` (Payload: "ON" atau "OFF")
*   `sg/aktuator/pompa` (Payload: "ON" atau "OFF")

**3. Topik Status Sistem & Keamanan (Fail-Safe)**
*   `sg/sistem/koneksi` (Payload: "ONLINE" atau "OFFLINE" - menggunakan LWT)
*   `sg/sistem/mode` (Payload: "NORMAL", "FAIL-SAFE", atau "EMERGENCY_STOP")
*   `sg/sistem/error` (Payload: string pesan error)

## Struktur Folder Repositori
*   `/esp32_firmware` - Untuk source code mikrokontroler ESP32 (Tugas Harits)
*   `/dashboard` - Untuk source code antarmuka pemantauan (Tugas Fathoni)
*   `/hardware` - Untuk skematik dan desain mekanik (Tugas Bagus)
*   `/docs` - Dokumentasi dan laporan proyek

## Cara Mengetes Komunikasi (Simulasi)
1. Install [MQTT Explorer](http://mqtt-explorer.com/).
2. Buat koneksi ke `broker.emqx.io` port `1883`.
3. Lakukan subscribe ke topik `sg/#`.
4. Coba publish data ke `sg/sensor/suhu` dan pantau hasilnya.
