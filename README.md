# Hydromate 🌱

Sistem hidroponik cerdas berbasis IoT untuk monitoring dan kontrol nutrisi tanaman secara otomatis dan real-time.

## 📌 Latar Belakang

Pengelolaan nutrisi hidroponik (pH, TDS/PPM, suhu air) sering tidak stabil jika dilakukan manual.  
Kesalahan kecil bisa bikin gagal panen atau boros nutrisi. Hydromate hadir sebagai solusi otomatis dan terukur.

## 🎯 Tujuan

- Menjaga pH dan nutrisi sesuai kebutuhan tanaman
- Monitoring kondisi hidroponik secara real-time
- Mengurangi human error dan pemborosan
- Meningkatkan efisiensi dan hasil panen

## ⚙️ Fitur Utama

- Monitoring pH, TDS/PPM, suhu air
- Kontrol otomatis pompa nutrisi A & B
- Kontrol pH up & down
- Mode manual & otomatis
- Logging data ke cloud
- Notifikasi jika terjadi anomali

## 🧠 Arsitektur Singkat

- **Device**: ESP32 + sensor (pH, TDS, DS18B20, waterflow)
- **Backend/Cloud**: Firebase
- **Frontend**: Mobile Dashboard
