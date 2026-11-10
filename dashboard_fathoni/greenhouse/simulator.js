// Simulator ESP32: meniru topic, QoS, retained, dan payload persis sesuai SPEC.md
// Pakai: npm run sim -- <skenario>
// normal | hot | dry | sensor-fault | estop | disconnect | packet-loss | restart | no-ntp | hang
import mqtt from 'mqtt';

const sc = process.argv[2] || 'normal';
const { MQTT_URL = 'mqtt://localhost:1883', MQTT_USER, MQTT_PASS, PUBLISH_INTERVAL_MS = 2000 } = process.env;
const T = { sensor: 'greenhouse/sensor/data', act: 'greenhouse/actuator/status', sys: 'greenhouse/system/status', alert: 'greenhouse/alert' };
const FAN_ON = 35, FAN_OFF = 28, PUMP_ON = 30, PUMP_OFF = 60; // sama dengan SPEC (dikunci di firmware)

// Last Will: broker mengirim "offline" otomatis jika koneksi putus tidak normal
const c = mqtt.connect(MQTT_URL, {
  username: MQTT_USER || undefined, password: MQTT_PASS || undefined,
  will: { topic: T.sys, payload: JSON.stringify({ state: 'offline', sensor_fault: false }), qos: 1, retain: true },
});
const pub = (t, o, retain = false) => { c.publish(t, JSON.stringify(o), { qos: 1, retain }); console.log(t.padEnd(28), JSON.stringify(o)); };
const now = () => Math.floor(Date.now() / 1000);

let temp = sc === 'hot' ? 33 : 30, soil = sc === 'dry' ? 35 : 50, hum = 65;
let fan = false, pump = false, fault = false, estop = false, seq = 0, n = 0, lastAct = '', faultSent = false;
const alerted = {};
const alertOnce = (code, level, message, active) => {   // cooldown: kirim sekali per kemunculan kondisi
  if (active && !alerted[code]) { alerted[code] = true; pub(T.alert, { level, code, message, ts: now() }); }
  if (!active) alerted[code] = false;
};

function tick() {
  n++;
  if (sc === 'hang' && n > 15) return;                  // tetap online tapi berhenti kirim data
  if (sc === 'disconnect' && n > 10) {                  // putus paksa tanpa disconnect bersih -> Last Will terpicu
    c.options.reconnectPeriod = 0; c.stream.destroy(); setTimeout(() => process.exit(0), 500); return;
  }
  // Dinamika fisik sederhana: kipas menurunkan suhu, pompa menaikkan kelembapan tanah
  temp += (fan ? -0.7 : sc === 'hot' ? 0.9 : 0.15) + (Math.random() - 0.5) * 0.3;
  soil = Math.max(0, Math.min(100, soil + (pump ? 2 : sc === 'dry' ? -1.5 : -0.4)));
  hum = Math.max(30, Math.min(95, hum + (Math.random() - 0.5) * 1.5));
  if (sc === 'sensor-fault' && n > 8) fault = true;
  if (sc === 'estop' && n > 8) estop = true;
  if (fault || estop) { fan = false; pump = false; }    // aktuator diputus
  else { // histeresis: ON di atas ambang, OFF baru setelah melewati ambang bawah
    if (!fan && temp > FAN_ON) fan = true; else if (fan && temp <= FAN_OFF) fan = false;
    if (!pump && soil < PUMP_ON) pump = true; else if (pump && soil >= PUMP_OFF) pump = false;
  }
  const ts = sc === 'no-ntp' && n <= 15 ? 0 : now();
  const mode = estop ? 'emergency' : 'auto';
  const sig = [fan, pump, mode, estop].join();
  if (sig !== lastAct) { lastAct = sig; pub(T.act, { fan, pump, mode, estop, ts }, true); } // hanya saat berubah
  if (fault && !faultSent) { faultSent = true; pub(T.sys, { state: 'online', sensor_fault: true }, true); }
  alertOnce('TEMP_CRITICAL', 'critical', `Suhu ${temp.toFixed(1)}°C melebihi batas ${FAN_ON}°C`, !fault && temp > FAN_ON);
  alertOnce('SOIL_DRY', 'warning', `Kelembapan tanah ${Math.round(soil)}% di bawah ${PUMP_ON}%`, !fault && !estop && soil < PUMP_ON);
  alertOnce('SENSOR_FAULT', 'critical', 'Pembacaan DHT22 tidak valid', fault);
  alertOnce('ESTOP', 'critical', 'Emergency Stop aktif', estop);
  if (sc === 'restart' && n === 12) seq = 0;            // meniru ESP32 reboot
  if (sc === 'packet-loss' && n % 5 === 0) seq += 2;    // lewati 2 nomor seq
  pub(T.sensor, { temp: fault ? null : +temp.toFixed(1), hum: fault ? null : +hum.toFixed(1), soil: Math.round(soil), seq, ts });
  seq++;
}

c.on('connect', () => {
  console.log(`Simulator terhubung, skenario: ${sc}`);
  pub(T.sys, { state: 'online', sensor_fault: false }, true);
  setInterval(tick, +PUBLISH_INTERVAL_MS);
});
process.on('SIGINT', () => {                            // shutdown bersih
  pub(T.sys, { state: 'offline', sensor_fault: false }, true);
  c.end(false, {}, () => process.exit(0));
});
