# Smart Greenhouse Dashboard

Alur: ESP32 (atau simulator) -> Mosquitto -> backend (validasi + SQLite) -> dashboard (live via WebSocket + riwayat via REST).

## Menjalankan (PowerShell, perlu Node 20+ dan Docker Desktop)
```
docker compose up -d          # 1. broker
npm install                   # 2. sekali saja
npm start                     # 3. backend + dashboard -> http://localhost:3000
npm run sim -- normal         # 4. (terminal lain) simulator ESP32
```
Skenario simulator: `normal`, `hot`, `dry`, `sensor-fault`, `estop`, `disconnect`, `packet-loss`, `restart`, `no-ntp`, `hang`.
Dengan ESP32 asli: hentikan simulator, arahkan ESP32 ke IP komputer ini port 1883 dengan topic sesuai SPEC.

Variabel opsional: `MQTT_URL`, `PORT`, `DB_FILE`, `STALE_THRESHOLD_MS` (backend); `PUBLISH_INTERVAL_MS` (simulator).
Ambang di dashboard (35/28/30/60 dan 7 detik basi) ada di bagian `CFG` pada `public/index.html`.

## Endpoint
`/api/latest` `/api/readings?range=1h|6h|24h|7d` `/api/alerts` `/api/events` `/api/health`
