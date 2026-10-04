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
Dokumentasi proses pengerjaan dibagi berdasarkan pembagian tugas tiap anggota:

#### 1. Hardware & Mechanical System (Bagus Satria Priambodo)
* **Deskripsi Pengerjaan:**
  * Merancang skematik pengkabelan sistem (*Schematic Revisi*) dengan mengintegrasikan modul LM2596 Buck Converter dan rangkaian transistor BJT sebagai pengganti relay.
  * Melakukan perakitan jalur catu daya 12V, 5V, dan 3.3V pada breadboard serta instalasi saklar pengaman (*Rocker Switch*).
  * Menguji aktuator Kipas DC 12V dan Pompa Air Mini 5V.
* **Dokumentasi:** *(Foto perakitan breadboard & skematik)*

#### 2. Embedded & Control System (Muhammad Harits Arrosyid)
* **Deskripsi Pengerjaan:**
  * Menulis dan mengembangkan firmware kontrol ESP32 menggunakan Arduino IDE.
  * Mengimplementasikan pembacaan sensor DHT22 (suhu udara) dan sensor Soil Moisture kapasitif.
  * Melakukan eksperimen kalibrasi nilai ADC sensor tanah pada kondisi kering dan basah.
  * Mengintegrasikan algoritma kontrol *Dual Closed-Loop* untuk otomatisasi kipas dan pompa air.
* **Dokumentasi:** *(Foto proses pemrograman & pengujian sensor)*

#### 3. Network Communication & IoT (Aditya Rahman)
* **Deskripsi Pengerjaan:**
  * **Tahap 1 (Simulasi / Mock Testing):** Merancang arsitektur komunikasi dan struktur topik hierarkis (`sg/sensor/...`, `sg/aktuator/...`, `sg/sistem/...`) serta menguji konsep Publish/Subscribe menggunakan aplikasi MQTTX Client.
  * **Tahap 2 (Integrasi Hardware ke Broker):** Mengunggah firmware ke ESP32 fisik via USB-TTL, menghubungkannya ke WiFi lokal, dan memverifikasi pengiriman data telemetri periodik ke Cloud Broker `broker.emqx.io`.
  * **Tahap 3 (Komunikasi 2 Arah & Fail-Safe):** Menguji penerimaan perintah aktuator (*Subscribe*) dari luar (`sg/aktuator/kipas : ON`) serta memverifikasi ketahanan jaringan dengan fitur *Auto-Reconnect* otomatis saat WiFi drop.
* **Dokumentasi:** *(Tangkapan layar Serial Monitor Arduino IDE bersisian dengan MQTTX Client dari presentasi PPT)*

#### 4. Monitoring Interface & Evaluation (Fathoni Ibra A A)
* **Deskripsi Pengerjaan:**
  * Menentukan parameter pemantauan dan format data telemetri dari topik-topik MQTT.
  * Merancang antarmuka awal (*UI Dashboard*) untuk menampilkan grafik suhu, kelembaban, dan status aktuator secara real-time.
  * Menyiapkan integrasi subscriber dashboard dengan MQTT broker.
* **Dokumentasi:** *(Tangkapan layar rancangan UI Dashboard)*

---

### 1.3 Dokumentasi Kondisi Terakhir Sistem
Hingga tanggal 4 Oktober 2026, kondisi terakhir sistem adalah sebagai berikut:
1. **Kondisi Perangkat Keras (Hardware):** Modul mikrokontroler fisik (ESP32) aktif beroperasi, terhubung dengan catu daya, dan berhasil mengeksekusi instruksi pembacaan serta pemancar data telemetri secara stabil.
2. **Kondisi Jaringan (Cyber/IoT):** Komunikasi data dua arah antara mikrokontroler dan Cloud MQTT Broker (`broker.emqx.io:1883`) berjalan secara real-time, lengkap dengan fitur *Fail-Safe Auto-Reconnect*.
3. **Verifikasi Operasional Langsung:** Seluruh sistem komunikasi dan firmware diverifikasi dan dioperasikan secara langsung oleh penanggung jawab subsistem Network & IoT (Aditya Rahman) di depan perangkat keras dan layar pemantau live.

*(Foto dokumentasi: Aditya di depan laptop yang menampilkan Serial Monitor + MQTTX aktif, dengan modul ESP32 di atas meja)*

---

## BAB II: ANALISA CAPAIAN SAAT INI DIBANDINGKAN RENCANA

### 2.1 Kesesuaian Jadwal & Rancangan Sistem
Secara umum proyek berjalan **sesuai jadwal (*on-track*)** untuk aspek siber, jaringan, dan kontrol embedded. Penyesuaian dilakukan pada modul regulator daya (menggunakan LM2596 Buck Converter) dan penggerak transistor untuk meningkatkan efisiensi dan keamanan termal sistem.

### 2.2 Realisasi Pembagian Kerja Tim (Target vs Aktual)
1. **Bagus (Hardware & Mechanical):** Selesai membuat wiring diagram dan perakitan komponen utama. Pembuatan fisik maket greenhouse dijadwalkan pasca-UTS untuk menyesuaikan dimensi akhir rangkaian.
2. **Harits (Embedded & Control):** Selesai mengimplementasikan pembacaan sensor DHT22 & Soil Moisture, kalibrasi nilai ADC, dan logika kontrol *Dual Closed-Loop*.
3. **Aditya (Network & IoT):** Selesai membangun arsitektur MQTT, struktur topik, verifikasi hardware fisik, komunikasi dua arah, dan pengujian *Network Fail-Safe*.
4. **Fathoni (Monitoring & Evaluation):** Selesai merancang format data dan konsep UI dashboard monitoring IoT.

### 2.3 Kesesuaian Alat dan Bahan
| Komponen Direncanakan | Komponen Aktual | Status | Keterangan |
| :--- | :--- | :---: | :--- |
| ESP32 Dev Module | ESP32 Dev Module & ESP32-C3 SuperMini | Sesuai | Dilengkapi board cadangan untuk redundansi. |
| Sensor DHT22 | Sensor DHT22 | Sesuai | Sensor suhu & kelembaban udara akurat. |
| Soil Moisture Sensor | Capacitive Soil Moisture Sensor v1.2 | Sesuai | Tipe kapasitif tahan korosi. |
| Regulator 7805 | LM2596 Buck Converter Module | Direvisi | Lebih efisien & tidak panas dibanding 7805. |
| Relay 2-Channel | Relay & Driver Transistor BJT | Sesuai | Respon cepat & opsi hemat daya. |
| Kipas DC 12V | Kipas DC 12V | Sesuai | Aktuator pendingin greenhouse. |
| Pompa Air Mini 5V | Pompa Air Mini Submersible 5V | Sesuai | Aktuator irigasi kelembaban tanah. |
| Emergency Stop | Rocker Switch / Tombol Safety | Sesuai | Pengaman fisik darurat pada jalur 12V. |

---

## BAB III: KENDALA DAN UPAYA PENYELESAIAN

### 3.1 Kendala yang Dihadapi
1. Port micro-USB ESP32 utama sempat terlepas.
2. Nilai pembacaan ADC sensor kelembaban tanah sempat hanya bernilai 0 dan 4095.
3. ESP32 gagal menyambung kembali ke MQTT (error rc=-2) saat Hotspot dimatikan-dinyalakan.

### 3.2 Upaya Mengatasi Kendala
1. Flashing firmware dilakukan menggunakan adapter USB-to-TTL serial eksternal (skema kabel silang dan boot mode manual) serta redundansi pengujian menggunakan ESP32-C3 SuperMini.
2. Memperbaiki pin ADC ke GPIO 4 (bukan GPIO 34 digital DHT22) dan mengalibrasi rentang nilai analog basah/kering.
3. Menerapkan mekanisme pemulihan 2 tahap pada firmware (re-koneksi WiFi terlebih dahulu hingga status connected sebelum menyambung kembali ke MQTT broker).

---

## BAB IV: RENCANA PENGERJAAN BERIKUTNYA

### 4.1 Target Utama Kelompok (Pasca UTS)
Integrasi sistem menyeluruh (*Full System Integration*): menyatukan seluruh komponen sensor, mikrokontroler, dan aktuator ke dalam maket akrilik greenhouse mini, serta menghubungkannya ke dashboard monitoring live.

### 4.2 Rencana Kerja Per-Anggota:
* **Bagus:** Fabrikasi maket fisik akrilik greenhouse dan instalasi mekanik kipas/pompa.
* **Harits:** Integrasi pembacaan sensor simultan dan penambahan batas *hysteresis* kontrol.
* **Aditya:** Pengujian latensi, *packet loss*, dan stabilitas QoS MQTT jangka panjang.
* **Fathoni:** Finalisasi antarmuka dashboard IoT dan visualisasi grafik data live.
