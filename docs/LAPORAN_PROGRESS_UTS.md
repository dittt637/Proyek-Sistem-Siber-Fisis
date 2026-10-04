# LAPORAN PERKEMBANGAN PROYEK CYBER PHYSICAL SECURITY
## SISTEM KONTROL DAN MONITORING SMART GREENHOUSE

* **Mata Kuliah:** Cyber Physical Security
* **Tanggal Laporan:** 4 Oktober 2026
* **Kelompok:**
  1. Bagus Satria Priambodo (Hardware & Mechanical)
  2. Muhammad Harits Arrosyid (Embedded & Control)
  3. Aditya Rahman (Network & IoT)
  4. Fathoni Ibra A A (Monitoring & Evaluation)

---

## BAB I: CAPAIAN PROYEK SAAT INI

### 1.1 Ringkasan Capaian Global
Hingga periode Minggu ke-7 (menjelang UTS), perkembangan proyek Smart Greenhouse berbasis Cyber Physical System (CPS) secara keseluruhan telah mencapai estimasi **65% - 70%**. 

Fondasi sistem siber dan fisik telah berhasil dibangun dan diuji secara terpisah maupun terintegrasi:
* Arsitektur komunikasi data (MQTT) dan mekanisme pemulihan jaringan (*fail-safe auto-reconnect*) telah tuntas 100%.
* Firmware mikrokontroler untuk pembacaan sensor (*DHT22 & Soil Moisture*) dan logika kendali tertutup (*Dual Closed-Loop*) telah selesai dikodekan.
* Skematik pengkabelan (*wiring diagram*) daya dan aktuator telah direvisi menggunakan konverter daya efisien (LM2596) dan penggerak transistor.
* Tahap integrasi maket fisik greenhouse dan penyempurnaan antarmuka dashboard monitoring saat ini sedang berlangsung.

### 1.2 Dokumentasi Proses Pengerjaan
1. **Perakitan & Wiring Komponen (Bagus):** Dokumentasi perakitan sirkuit sensor, ESP32, dan modul penggerak aktuator pada breadboard sesuai Schematic Revisi.
2. **Pemrograman & Kalibrasi Sensor (Harits):** Dokumentasi pengujian kalibrasi sensor kelembaban tanah dan pembacaan DHT22 di Arduino IDE.
3. **Pengujian Jaringan & MQTT (Aditya):** Foto dokumentasi pengujian komunikasi broker menggunakan ESP32 fisik (ESP32-C3 SuperMini) berdampingan dengan laptop yang menjalankan MQTTX dan Serial Monitor.
4. **Desain Antarmuka (Fathoni):** Tangkapan layar rancangan awal UI dashboard monitoring.

### 1.3 Dokumentasi Kondisi Terakhir Sistem
* **Perangkat Keras Terkini:** Rangkaian ESP32 fisik aktif terhubung daya dengan LED indikator menyala.
* **Tangkapan Layar Pengujian End-to-End:** Tangkapan layar Serial Monitor Arduino IDE yang menunjukkan data suhu dan kelembaban tanah berhasil terkirim (*Publish*), serta aplikasi MQTTX Client yang menerima data tersebut secara *real-time*.

---

## BAB II: ANALISA CAPAIAN SAAT INI DIBANDINGKAN RENCANA

### 2.1 Kesesuaian Jadwal & Rancangan Sistem
Secara umum, pelaksanaan proyek berjalan **sesuai jadwal (*on-track*)** untuk aspek fungsionalitas siber, jaringan, dan logika kontrol embedded. 

Namun, terdapat **penyesuaian rancangan perangkat keras (*hardware redesign*)** dari rencana awal:
1. **Sistem Regulasi Daya:** Mengganti regulator linier (LM7805) dengan modul switching **LM2596 Step-Down Buck Converter** untuk mencegah panas berlebih (*overheating*) serta efisiensi arus listrik.
2. **Driver Aktuator:** Menambahkan alternatif rangkaian transistor BJT dengan proteksi dioda flyback di samping modul relay untuk efisiensi konsumsi daya.
3. **Penyelesaian Fisik Greenhouse:** Pembuatan maket akrilik greenhouse mini sengaja digeser setelah UTS agar dimensi maket dapat disesuaikan secara presisi dengan tata letak akhir komponen elektronik.

### 2.2 Realisasi Pembagian Kerja Tim (Target vs Aktual)

#### 1. Bagus (Hardware & Mechanical)
* **Target (Minggu 4 - 7):** Menyelesaikan desain greenhouse fisik, menyusun diagram pengkabelan (*wiring diagram*), pengadaan komponen, serta perakitan sirkuit daya dan aktuator lengkap.
* **Aktual / Realisasi:** Pembuatan skematik pengkabelan (*Schematic Revisi*) dan perakitan komponen utama (ESP32, modul daya LM2596, sensor, saklar On/Off, dan aktuator) telah selesai dirangkai. Pembuatan fisik maket greenhouse akrilik ditunda sementara untuk memastikan kestabilan dimensi dan jalur kabel komponen terlebih dahulu.

#### 2. Harits (Embedded & Control)
* **Target (Minggu 4 - 7):** Menentukan kebutuhan sensor, implementasi pembacaan DHT22 dan Soil Moisture, kalibrasi nilai sensor, serta penyusunan algoritma kontrol *dual closed-loop*.
* **Aktual / Realisasi:** Telah menyelesaikan implementasi pembacaan sensor DHT22 (suhu udara) dan sensor kelembaban tanah kapasitif. Kalibrasi rentang nilai basah/kering (*ADC mapping*) telah diimplementasikan, serta logika otomatisasi kipas (>35°C ON, ≤28°C OFF) dan pompa air (<30% ON, ≥60% OFF) telah terintegrasi di firmware ESP32.

#### 3. Aditya (Network & IoT)
* **Target (Minggu 4 - 7):** Mendesain arsitektur komunikasi MQTT, menentukan hierarki topik, menyiapkan broker, menguji komunikasi dua arah (*Publish/Subscribe*), serta mekanisme *Fail-Safe* jaringan.
* **Aktual / Realisasi:** Berhasil membangun komunikasi MQTT berbasis Cloud Broker (`broker.emqx.io:1883`) dengan struktur topik hierarkis (`sg/sensor/...`, `sg/aktuator/...`, `sg/sistem/...`). Pengujian komunikasi dua arah dan verifikasi *Network Fail-Safe* (fitur *Auto-Reconnect* otomatis saat WiFi drop) telah teruji 100% menggunakan hardware fisik ESP32.

#### 4. Fathoni (Monitoring & Evaluation)
* **Target (Minggu 4 - 7):** Menentukan parameter monitoring, membuat rancangan awal dashboard visual, serta menguji integrasi penerimaan data dari MQTT broker.
* **Aktual / Realisasi:** Telah menentukan format data dan topik yang akan divisualisasikan. Rancangan antarmuka monitoring untuk visualisasi grafik suhu dan status aktuator telah disiapkan dan siap menerima *data stream* dari broker MQTT.

### 2.3 Kesesuaian Alat dan Bahan

| Komponen yang Direncanakan | Komponen Aktual / Realisasi | Status | Keterangan / Analisis Perubahan |
| :--- | :--- | :---: | :--- |
| **Mikrokontroler ESP32 Dev Module** | ESP32 Dev Module & ESP32-C3 SuperMini | Sesuai | Dilengkapi board cadangan (ESP32-C3) untuk redundansi pengujian. |
| **Sensor Suhu DHT22** | Sensor DHT22 | Sesuai | Akurasi tinggi untuk suhu udara greenhouse. |
| **Sensor Kelembaban Tanah** | Capacitive Soil Moisture Sensor v1.2 | Sesuai | Tipe kapasitif dipilih karena tahan korosi dibanding tipe resistif. |
| **Regulator Tegangan Linier (7805)** | Modul LM2596 Buck Converter | Direvisi | Diganti modul Buck Converter agar lebih efisien dan tidak panas. |
| **Modul Relay 2-Channel** | Modul Relay & Driver Transistor BJT | Sesuai | Ditambahkan opsi transistor driver untuk respon switching cepat. |
| **Kipas DC 12V** | Kipas DC 12V | Sesuai | Digunakan sebagai aktuator pendingin. |
| **Pompa Air Mini 5V** | Pompa Air Submersible 5V | Sesuai | Digunakan sebagai aktuator irigasi tanaman. |
| **Emergency Stop Button** | Rocker Switch / Tombol Safety | Sesuai | Dipasang seri pada jalur daya utama 12V. |

---

## BAB III: KENDALA DAN UPAYA PENYELESAIAN

### 3.1 Kendala yang Dihadapi
1. **Port Micro-USB ESP32 Rusak/Copot:** Port fisik micro-USB pada modul ESP32 utama sempat terlepas, sehingga proses flashing kode program melalui kabel USB bawaan tidak dapat dilakukan secara langsung.
2. **Nilai Pembacaan Sensor Tanah Tidak Linear (Hanya 0 dan 4095):** Pada saat awal kalibrasi sensor kelembaban tanah, nilai ADC yang terbaca di Serial Monitor hanya menampilkan angka ekstrem `0` atau `4095`.
3. **Koneksi Jaringan Tidak Otomatis Pulih (*Error rc=-2*):** Saat dilakukan uji coba pemutusan Hotspot/WiFi, ESP32 gagal melakukan koneksi ulang ke broker MQTT dan terjebak dalam perulangan error `rc=-2`.

### 3.2 Upaya Mengatasi Kendala
1. **Penyelesaian Port USB:** Dilakukan pengunggahan firmware secara manual menggunakan konverter eksternal **USB-to-TTL Serial** dengan skema kabel silang (TX ke RX0, RX ke TX0) serta mengaktifkan *Boot Mode* manual (GPIO 0 ke GND). Selain itu, disediakan modul cadangan **ESP32-C3 SuperMini** untuk memperlancar proses pengujian jaringan.
2. **Penyelesaian Sensor Tanah:** Dilakukan penyesuaian pemetaan pin ADC mikrokontroler (menggunakan GPIO 4) serta memastikan pembacaan dilakukan pada pin Analog Output (AO), bukan Digital Output (DO). Dilakukan pula kalibrasi pemetaan nilai (*mapping function*) dari kondisi kering di udara ke kondisi basah di air.
3. **Penyelesaian Jaringan Fail-Safe:** Algoritma firmware diperbarui dengan **mekanisme pemulihan 2 tahap**. Sistem memprogram ESP32 untuk mendeteksi status WiFi terlebih dahulu dan melakukan inisialisasi ulang sambungan WiFi (`WiFi.reconnect()`) sebelum mencoba menghubungkan kembali ke MQTT Broker.

---

## BAB IV: RENCANA PENGERJAAN BERIKUTNYA

### 4.1 Target Utama Kelompok (Pasca UTS)
Target utama kelompok setelah periode UTS adalah **Integrasi Sistem Menyeluruh (Full System Integration)**, yaitu menyatukan hardware fisik yang telah terpasang sensor dan aktuator ke dalam maket greenhouse mini, lalu menghubungkannya secara penuh ke dashboard monitoring jarak jauh dan melakukan evaluasi keandalan CPS selama operasi berkelanjutan.

### 4.2 Rencana Kerja Per-Anggota:
* **Bagus (Hardware & Mechanical):**
  * Memulai fabrikasi maket fisik greenhouse mini (pemotongan akrilik dan tata letak mekanik).
  * Merapikan pengkabelan dari breadboard ke PCB/box enclosure dan memasang aktuator kipas serta selang irigasi pompa secara permanen.
* **Harits (Embedded & Control):**
  * Mengintegrasikan pembacaan sensor fisik DHT22 dan Soil Moisture ke dalam firmware utama secara simultan.
  * Melakukan *fine-tuning* batas ambang kendali (*hysteresis control*) agar aktuator tidak mengalami *bouncing* (sering mati-hidup dalam waktu singkat).
* **Aditya (Network & IoT):**
  * Melakukan pengujian performa jaringan (pengukuran latensi pengiriman data, *packet loss*, dan evaluasi stabilitas QoS MQTT).
  * Membantu integrasi *data stream* MQTT ke backend/dashboard monitoring yang dibangun Fathoni.
* **Fathoni (Monitoring & Evaluation):**
  * Menyelesaikan pembuatan antarmuka dashboard monitoring IoT (menampilkan grafik data suhu, kelembaban, serta indikator status kipas/pompa).
  * Melakukan pengujian penerimaan data *real-time* dari broker MQTT serta menyusun metrik evaluasi kinerja CPS.
