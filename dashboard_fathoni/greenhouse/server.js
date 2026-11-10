// Backend: subscribe MQTT -> validasi sesuai SPEC.md -> simpan SQLite -> REST API + sajikan dashboard
import mqtt from 'mqtt';
import express from 'express';
import Database from 'better-sqlite3';
import path from 'path';

const { MQTT_URL = 'mqtt://localhost:1883', MQTT_WS_URL = '', MQTT_USER, MQTT_PASS, PORT = 3000, DB_FILE = 'greenhouse.db' } = process.env;

const db = new Database(DB_FILE);
db.exec(`
CREATE TABLE IF NOT EXISTS readings(rx INTEGER, ts INTEGER, temp REAL, hum REAL, soil INTEGER, seq INTEGER);
CREATE TABLE IF NOT EXISTS alerts(rx INTEGER, ts INTEGER, level TEXT, code TEXT, message TEXT);
CREATE TABLE IF NOT EXISTS events(rx INTEGER, kind TEXT, data TEXT);
CREATE INDEX IF NOT EXISTS ix_readings_rx ON readings(rx);`);
const insReading = db.prepare('INSERT INTO readings VALUES(?,?,?,?,?,?)');
const insAlert = db.prepare('INSERT INTO alerts VALUES(?,?,?,?,?)');
const insEvent = db.prepare('INSERT INTO events VALUES(?,?,?)');

// ---- Validator per topic: kembalikan null jika valid, atau alasan penolakan ----
const int = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
const numOrNull = (v, lo, hi) => v === null || (typeof v === 'number' && v >= lo && v <= hi);
const validate = {
  'greenhouse/sensor/data': p =>
    !numOrNull(p.temp, -40, 80) ? 'temp tidak valid' :
    !numOrNull(p.hum, 0, 100) ? 'hum tidak valid' :
    !int(p.soil, 0, 100) ? 'soil bukan integer 0-100' :
    !int(p.seq, 0, 4294967295) ? 'seq tidak valid' :
    !int(p.ts, 0, 4e9) ? 'ts tidak valid' : null,
  'greenhouse/actuator/status': p =>
    typeof p.fan !== 'boolean' || typeof p.pump !== 'boolean' || typeof p.estop !== 'boolean' ? 'fan/pump/estop harus boolean' :
    !['auto', 'offline', 'emergency'].includes(p.mode) ? 'mode tidak valid' :
    !int(p.ts, 0, 4e9) ? 'ts tidak valid' : null,
  'greenhouse/system/status': p =>
    !['online', 'offline'].includes(p.state) ? 'state tidak valid' :
    typeof p.sensor_fault !== 'boolean' ? 'sensor_fault harus boolean' : null,
  'greenhouse/alert': p =>
    !['warning', 'critical'].includes(p.level) ? 'level tidak valid' :
    typeof p.code !== 'string' || typeof p.message !== 'string' ? 'code/message harus string' :
    !int(p.ts, 0, 4e9) ? 'ts tidak valid' : null,
};

// ---- Status terbaru + statistik packet loss / latensi ----
const S = { sensor: null, actuator: null, system: null };
const q = { got: 0, lost: 0, last: null, restarts: 0, rejected: 0, lat: [] };

function onSensor(p, rx) {
  // Aturan A1: ts=0 atau selisih >60 detik dari waktu server dianggap tidak terpercaya
  const tsOk = p.ts > 0 && Math.abs(p.ts - rx / 1000) <= 60;
  if (q.last !== null && p.seq <= q.last) {
    if (p.seq < q.last && p.seq <= 10) {          // seq turun ke awal = ESP32 restart (bukan packet loss)
      q.restarts++; q.got = 0; q.lost = 0;
      insEvent.run(rx, 'restart', JSON.stringify({ from: q.last, to: p.seq }));
    } else return;                                 // duplikat / tidak berurutan (QoS 1): abaikan
  } else if (q.last !== null) q.lost += p.seq - q.last - 1; // celah seq = pesan hilang
  q.last = p.seq; q.got++;
  if (tsOk) { q.lat.push(rx - p.ts * 1000); if (q.lat.length > 500) q.lat.shift(); }
  insReading.run(rx, p.ts, p.temp, p.hum, p.soil, p.seq);
  S.sensor = { ...p, rx, tsOk };
}

const stats = () => {
  const tot = q.got + q.lost, L = q.lat;
  return {
    received: q.got, lost: q.lost, lossPct: tot ? +(q.lost / tot * 100).toFixed(2) : 0,
    restarts: q.restarts, rejected: q.rejected,
    latAvgMs: L.length ? Math.round(L.reduce((a, b) => a + b, 0) / L.length) : null,
    latMaxMs: L.length ? Math.max(...L) : null,
    note: 'ts beresolusi 1 detik, latensi hanya akurat sekitar +/-1 detik',
  };
};

// ---- MQTT ----
const c = mqtt.connect(MQTT_URL, { username: MQTT_USER || undefined, password: MQTT_PASS || undefined });
c.on('connect', () => { console.log('MQTT terhubung'); c.subscribe('greenhouse/#', { qos: 1 }); });
c.on('close', () => console.log('MQTT terputus, mencoba menyambung ulang...'));
c.on('error', e => console.warn('MQTT error:', e.message));
c.on('message', (topic, buf) => {
  const rx = Date.now();
  let p;
  try { p = JSON.parse(buf.toString()); } catch { q.rejected++; return console.warn('TOLAK (bukan JSON):', topic); }
  const check = validate[topic];
  const bad = !p || typeof p !== 'object' ? 'payload bukan objek' : check ? check(p) : 'topic tidak dikenal';
  if (bad) { q.rejected++; return console.warn('TOLAK', topic, '-', bad); } // pesan rusak tidak disimpan, backend tetap hidup
  if (topic === 'greenhouse/sensor/data') onSensor(p, rx);
  else if (topic === 'greenhouse/actuator/status') { S.actuator = p; insEvent.run(rx, 'actuator', JSON.stringify(p)); }
  else if (topic === 'greenhouse/system/status') { S.system = p; insEvent.run(rx, 'system', JSON.stringify(p)); }
  else if (topic === 'greenhouse/alert') insAlert.run(rx, p.ts, p.level, p.code, p.message);
});

// ---- REST API + dashboard statis ----
const app = express();
app.use(express.static('public'));
// mqtt.js disajikan dari node_modules agar dashboard jalan tanpa internet
app.get('/vendor/mqtt.js', (_, r) => r.sendFile(path.resolve('node_modules/mqtt/dist/mqtt.min.js')));
const RANGE = { '1h': 36e5, '6h': 216e5, '24h': 864e5, '7d': 6048e5 };
app.get('/api/latest', (_, r) => r.json({ ...S, stats: stats() }));
app.get('/api/readings', (req, r) => {
  const since = Date.now() - (RANGE[req.query.range] || RANGE['1h']);
  let rows = db.prepare('SELECT rx,temp,hum,soil FROM readings WHERE rx>? ORDER BY rx').all(since);
  const k = Math.ceil(rows.length / 500);          // downsampling maksimal ~500 titik
  if (k > 1) rows = rows.filter((_, i) => i % k === 0);
  r.json(rows);
});
const limit = req => Math.min(+req.query.limit || 50, 200);
app.get('/api/alerts', (req, r) => r.json(db.prepare('SELECT * FROM alerts ORDER BY rx DESC LIMIT ?').all(limit(req))));
app.get('/api/events', (req, r) => r.json(db.prepare('SELECT * FROM events ORDER BY rx DESC LIMIT ?').all(limit(req))));
// alamat WebSocket broker untuk dashboard (diatur lewat .env, bukan hardcode)
app.get('/api/config', (_, r) => r.json({ wsUrl: MQTT_WS_URL, username: MQTT_USER || '', password: MQTT_PASS || '' }));
app.get('/api/health', (_, r) => r.json({ ok: true, mqtt: c.connected }));
app.listen(PORT, () => console.log(`Dashboard: http://localhost:${PORT}`));
