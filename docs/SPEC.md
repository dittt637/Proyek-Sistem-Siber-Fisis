# SPEC.md – Spesifikasi Sistem Smart Greenhouse

> ⚠️ **File ini adalah sumber kebenaran tunggal.**  
> Topic MQTT, struktur JSON payload, satuan unit, dan aturan validasi **TIDAK BOLEH diubah**
> tanpa persetujuan eksplisit dari pemilik proyek.  
> Jika ada perubahan yang diperlukan → catat di bagian **Keputusan & Asumsi** terlebih dahulu.

_Versi: 2.1.0 | Terakhir diperbarui: 2026-10-04_

---

## Daftar Isi

1. [Tujuan & Ruang Lingkup](#1-tujuan--ruang-lingkup)
2. [Parameter Monitoring](#2-parameter-monitoring)
3. [Kontrak Data MQTT](#3-kontrak-data-mqtt)
4. [Skenario Kondisi Sistem](#4-skenario-kondisi-sistem)
5. [Rancangan Layout Dashboard](#5-rancangan-layout-dashboard)
6. [Metrik Evaluasi](#6-metrik-evaluasi)
7. [Keputusan & Asumsi](#7-keputusan--asumsi)

---

## 1. Tujuan & Ruang Lingkup

### 1.1 Tujuan Dashboard

Dashboard ini adalah antarmuka monitoring dan kontrol **real-time** untuk sistem Smart Greenhouse
berbasis ESP32. Tujuan utamanya:

- Menampilkan nilai sensor secara langsung (live) dengan pembaruan otomatis setiap kali
  data MQTT diterima.
- Memberikan indikasi visual status aktuator (kipas dan pompa) dan mode operasi sistem.
- Menampilkan peringatan (alert) secara proaktif agar operator dapat segera merespons.
- Merekam riwayat data dan alert untuk keperluan analisis dan demo akademik.
- Mengukur performa komunikasi MQTT (packet loss dan latensi) pada minggu evaluasi (W13–W14).

### 1.2 Yang Termasuk dalam Ruang Lingkup

| Termasuk | Keterangan |
|----------|------------|
| Monitoring sensor real-time | Suhu udara, kelembapan udara, kelembapan tanah |
| Status aktuator | Kipas DC dan pompa air, mode operasi |
| Sistem alert | Peringatan dan kondisi kritis |
| Grafik historis | Riwayat suhu dan kelembapan tanah dalam satu sesi |
| Status koneksi | Online/Offline, sensor fault, E-Stop |
| Metrik MQTT | Packet loss dan estimasi latensi |

### 1.3 Yang TIDAK Termasuk dalam Ruang Lingkup

| Tidak Termasuk | Alasan |
|----------------|--------|
| Kontrol manual aktuator dari dashboard | Dashboard bersifat **read-only**; tidak ada command topic dalam kontrak |
| Konfigurasi threshold dari dashboard | Threshold dikunci di firmware; dashboard membaca dari variabel `.env` (`FAN_ON`, `FAN_OFF`, `PUMP_ON`, `PUMP_OFF`) |
| Autentikasi pengguna | Di luar cakupan proyek akademik ini |
| Notifikasi push/email | Di luar cakupan proyek akademik ini |

---

## 2. Parameter Monitoring

### 2.1 Definisi Parameter & Satuan

| Parameter | Simbol Field | Satuan | Sumber Sensor | Catatan |
|-----------|-------------|--------|---------------|---------|
| Suhu Udara | `temp` | °C | DHT22 | `null` jika sensor gagal |
| Kelembapan Udara | `hum` | % RH | DHT22 | `null` jika sensor gagal |
| Kelembapan Tanah | `soil` | % (0–100) | Soil Moisture Sensor | 0 = sangat kering, 100 = sangat basah |
| Status Kipas | `fan` | boolean | Relay output | `true` = ON |
| Status Pompa | `pump` | boolean | Relay output | `true` = ON |
| Mode Operasi | `mode` | enum | ESP32 state machine | `auto` / `offline` / `emergency` |
| E-Stop | `estop` | boolean | Hardware switch | `true` = E-Stop aktif |
| Status Koneksi | `state` | enum | MQTT LWT | `online` / `offline` |
| Sensor Fault | `sensor_fault` | boolean | ESP32 diagnostics | `true` = kabel sensor putus/tidak valid |

---

## 3. Kontrak Data MQTT

### 3.1 Ringkasan Topic

| # | Topic | QoS | Retained | Arah | Fungsi |
|---|-------|-----|----------|------|--------|
| 1 | `greenhouse/sensor/data` | 1 | `false` | ESP32 → Broker → Backend | Pengiriman data sensor periodik |
| 2 | `greenhouse/actuator/status` | 1 | `true` | ESP32 → Broker → Backend | Status aktuator dan mode operasi |
| 3 | `greenhouse/system/status` | 1 | `true` | ESP32 → Broker → Backend | Status koneksi + Last Will Testament |
| 4 | `greenhouse/alert` | 1 | `false` | ESP32 → Broker → Backend | Notifikasi peringatan dan kondisi kritis |

---

### 3.2 Topic 1: `greenhouse/sensor/data`

**Format Payload (JSON):**

```json
{
  "temp": 31.5,
  "hum": 68.2,
  "soil": 42,
  "seq": 1024,
  "ts": 1759500000
}
```

---

### 3.3 Topic 2: `greenhouse/actuator/status`

**Format Payload (JSON):**

```json
{
  "fan": true,
  "pump": false,
  "mode": "auto",
  "estop": false,
  "ts": 1759500000
}
```

---

### 3.4 Topic 3: `greenhouse/system/status`

**Format Payload (JSON) — Normal:**

```json
{
  "state": "online",
  "sensor_fault": false
}
```

**Format Payload (JSON) — Last Will / Disconnect:**

```json
{
  "state": "offline",
  "sensor_fault": false
}
```

---

### 3.5 Topic 4: `greenhouse/alert`

**Format Payload (JSON):**

```json
{
  "level": "warning",
  "code": "TEMP_CRITICAL",
  "message": "Suhu udara 36.2°C melebihi batas kritis 35°C",
  "ts": 1759500000
}
```

Daftar kode alert resmi: `TEMP_CRITICAL`, `SOIL_DRY`, `SENSOR_FAULT`, `ESTOP`.
