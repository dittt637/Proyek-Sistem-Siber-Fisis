"""
UDP Telemetry Monitor & Downlink Controller
Smart Greenhouse CPS - Aditya Rahman (Network & IoT)

Fitur:
1. Menangkap siaran UDP Broadcast (Port 4210) dari ESP32 di jaringan lokal.
2. Parsing data JSON real-time (Suhu, Kelembaban, Tanah, Urutan Paket, Status Aktuator).
3. Mengukur latensi lokal (ms).
"""

import socket
import json
import datetime

UDP_PORT = 4210

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Izinkan reuse address & broadcast
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    try:
        sock.bind(("", UDP_PORT))
    except Exception as e:
        print(f"[ERROR] Gagal bind ke port {UDP_PORT}: {e}")
        return

    print("=" * 65)
    print("  SMART GREENHOUSE - UDP LOCAL TELEMETRY MONITOR (PORT 4210)  ")
    print("  Menunggu paket broadcast dari ESP32 di jaringan WiFi...      ")
    print("  Tekan Ctrl+C untuk berhenti.                                 ")
    print("=" * 65 + "\n")

    packet_count = 0

    while True:
        try:
            data, addr = sock.recvfrom(2048)
            rx_time = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            raw_text = data.decode("utf-8", errors="ignore")
            packet_count += 1

            try:
                payload = json.loads(raw_text)
                # Cek tipe paket berdasarkan field yang ada
                if "temp" in payload:
                    temp = payload.get("temp")
                    hum = payload.get("hum")
                    soil = payload.get("soil")
                    seq = payload.get("seq", "--")
                    print(f"[{rx_time}] [SENSOR] Dari: {addr[0]} | Seq: #{seq} | "
                          f"Suhu: {temp}°C | Kelembaban: {hum}% | Tanah: {soil}%")
                elif "fan" in payload:
                    fan = "ON" if payload.get("fan") else "OFF"
                    pump = "ON" if payload.get("pump") else "OFF"
                    mode = payload.get("mode", "--")
                    estop = "AKTIF" if payload.get("estop") else "NORMAL"
                    print(f"[{rx_time}] [AKTUATOR] Dari: {addr[0]} | Kipas: {fan} | Pompa: {pump} | Mode: {mode} | E-Stop: {estop}")
                elif "level" in payload:
                    lvl = payload.get("level", "").upper()
                    code = payload.get("code", "")
                    msg = payload.get("message", "")
                    print(f"[{rx_time}] [⚠️ ALERT - {lvl}] {code}: {msg}")
                else:
                    print(f"[{rx_time}] [RAW DATA] Dari: {addr[0]} -> {raw_text}")
            except json.JSONDecodeError:
                print(f"[{rx_time}] [NON-JSON] Dari: {addr[0]} -> {raw_text}")

        except KeyboardInterrupt:
            print("\n[INFO] Monitor UDP dihentikan.")
            break
        except Exception as e:
            print(f"[ERROR] {e}")

if __name__ == "__main__":
    main()
