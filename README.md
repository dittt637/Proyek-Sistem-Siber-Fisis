# Sistem Pengendali Iklim dan Irigasi Smart Greenhouse

Proyek ini adalah implementasi **Cyber Physical System (CPS)** yang bertujuan untuk melakukan pemantauan dan pengendalian kondisi lingkungan *greenhouse* secara otomatis dengan arsitektur **Fail-Safe**.

## 👥 Tim Proyek
1. **Bagus Satria Priambodo** - Hardware & Mechanical
2. **Fathoni Ibra A A** - Monitoring & Evaluation
3. **Muhammad Harits Arrosyid** - Embedded & Control
4. **Aditya Rahman** - Network & IoT

---

## 📖 Deskripsi Umum Sistem
Sistem ini menggabungkan komponen fisik (greenhouse mini, sensor, aktuator) dengan komponen siber (mikrokontroler ESP32, komunikasi MQTT, dashboard monitoring). Sistem tidak hanya memantau, tetapi juga mampu mengambil keputusan berdasarkan kondisi yang terbaca oleh sensor untuk mengaktifkan aktuator secara otomatis.

Alur kerja sistem membentuk *feedback loop*: 
`Sensor membaca data` ➡️ `ESP32 memproses` ➡️ `Sistem menentukan tindakan` ➡️ `Aktuator merespon` ➡️ `Perubahan lingkungan dibaca kembali oleh sensor`.

---

## 🛠️ Komponen Utama Sistem

### Hardware
*   **Mikrokontroler ESP32:** Sebagai *controller* utama (*Edge Computing*) pembaca sensor dan logika kontrol.
*   **Sensor Suhu DHT22:** Membaca temperatur udara.
*   **Sensor Soil Moisture:** Membaca kelembaban/kadar air media tanam.
*   **Relay:** Saklar elektronik untuk aktuator daya besar.
*   **Kipas DC 12V:** Aktuator pendingin (menurunkan suhu ruangan).
*   **Pompa Air Mini 5V:** Aktuator penyiraman otomatis.

### Software & Network
*   **Protokol MQTT:** Mosquitto Broker untuk pertukaran data ringan dan tangguh dengan fitur QoS (Quality of Service).
*   **Koneksi:** WiFi 2.4GHz.

---

## 🔌 Skematik & Pemetaan Pin (Wiring Diagram)

Sistem menggunakan catu daya utama Adaptor 12V dengan **Emergency Stop Button** terpasang secara seri pada jalur utama VCC 12V. Tegangan kemudian diturunkan menggunakan regulator menjadi **5V** (untuk Pompa Air 5V & Modul Relay) dan **3.3V** (untuk ESP32, DHT22, dan Soil Moisture Sensor).

### Pemetaan Pin GPIO ESP32:
| Komponen Perangkat | Jenis / Fungsi | Pin Perangkat | Pin ESP32 |
| :--- | :--- | :--- | :--- |
| **DHT22** | Sensor Suhu & Kelembaban | Data | **GPIO 4** |
| **Capacitive Soil Moisture v1.2** | Sensor Kelembaban Tanah | Analog Out (AOUT) | **GPIO 34** |
| **Relay Channel 1** | Aktuator Pompa Air 5V | IN1 | **GPIO 27** |
| **Relay Channel 2** | Aktuator Kipas DC 12V | IN2 | **GPIO 26** |

*Dokumentasi lengkap dan diagram blok alur daya dapat dilihat di [hardware/README.md](hardware/README.md).*

---

## ⚙️ Sistem Kendali (Dual Closed-Loop)
Sistem menerapkan dua mekanisme kontrol tertutup:
1.  **Kontrol Suhu:** Jika suhu **> 35°C**, ESP32 akan menyalakan kipas. Jika suhu turun hingga **≤ 28°C**, kipas dimatikan.
2.  **Kontrol Irigasi (Tanah):** Jika kelembaban tanah **< 30%** (kering), ESP32 menyalakan pompa air. Setelah mencapai **≥ 60%**, pompa berhenti.

---

## 🛡️ Keamanan & Keandalan (Fail-Safe Architecture)
1.  **Edge Computing & Network Fail-Safe:** Jika koneksi MQTT atau jaringan internet terputus, ESP32 akan tetap menjalankan algoritma kontrol (menyiram & mendinginkan) secara lokal/mandiri. Aktuator tidak akan bergerak liar.
2.  **Fault Detection:** Mendeteksi anomali jika sensor rusak atau memberikan data yang tidak valid, sistem akan mematikan aktuator.
3.  **Emergency Stop:** Tombol pengaman fisik manual untuk menghentikan seluruh sistem pada kondisi darurat.

---

## 📡 Arsitektur Komunikasi MQTT

Sistem ini menggunakan arsitektur Publish/Subscribe untuk menjamin integritas pertukaran data antara lahan fisik dan *dashboard* jarak jauh.

*   **Broker yang digunakan:** `broker.emqx.io` (Public Broker untuk fase testing awal)
*   **Port:** `1883`

### Struktur Topik MQTT

**1. Topik Telemetri (Data Sensor dari ESP32)**
*   `sg/sensor/suhu` (Payload: float, misal `32.5`)
*   `sg/sensor/kelembaban_udara` (Payload: float, misal `60.0`)
*   `sg/sensor/kelembaban_tanah` (Payload: float, misal `45.0`)

**2. Topik Status Aktuator (Perubahan Status)**
*   `sg/aktuator/kipas` (Payload: `ON` atau `OFF`)
*   `sg/aktuator/pompa` (Payload: `ON` atau `OFF`)

**3. Topik Status Sistem & Keamanan (Prioritas QoS 1)**
*   `sg/sistem/koneksi` (Payload: `ONLINE` atau `OFFLINE` - menggunakan fitur *Last Will and Testament* / LWT dari Broker)
*   `sg/sistem/mode` (Payload: `NORMAL`, `FAIL-SAFE`, atau `EMERGENCY_STOP`)
*   `sg/sistem/error` (Payload: pesan error spesifik)

---

## 📂 Struktur Folder Repositori
*   `/esp32_firmware` - Untuk source code mikrokontroler ESP32
*   `/dashboard` - Untuk source code antarmuka pemantauan IoT
*   `/hardware` - Untuk skematik dan desain mekanik greenhouse
*   `/docs` - Dokumentasi dan laporan proyek
