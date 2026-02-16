SMART CLASS IOT ADVANCED v2.0
============================

LIBRARY YANG DIPERLUKAN (Arduino IDE):
- DHT sensor library (Adafruit)
- ArduinoJson 6.x (bintabnik atau arduinojson.org)
- BH1750 (by Christopher Laws) - sensor cahaya digital I2C
- UniversalTelegramBot (by Brian Lough)

BOARD: ESP32 Dev Module (DOIT DEVKIT V1)

FITUR BARU:
1. 25+ nada piano (50+ total) - oktaf 3, 4, 5, 6
2. Anti-bug: validasi NaN, bounds, safe defaults
3. Kalibrasi sensor: Gas, Suara (BH1750 tidak perlu kalibrasi - Lux nyata)
4. Data ilmiah HD: presisi tinggi, satuan SI
5. WiFi scan: lihat network tersedia
6. WiFi status JSON: IP, SSID, RSSI
7. Uptime & heap info
8. Sensor history: min/max/avg
9. Export JSON
10. Tema gelap/terang
11. REST API: /data, /scan, /wifi_status, /calibrate, /api/config
12. Threshold konfigurasi via web
13. Telegram Bot Control (/start, /info, /sensors, /kenyamanan, /mode)
14. Jadwal Bel Sekolah Otomatis (NTP)
15. Konversi Satuan Advance (C/F/K/R, Absolute Humid, Foot-candles, dll)

KONEKSI BH1750 (I2C):
- VCC -> 3.3V
- GND -> GND
- SCL -> GPIO 21
- SDA -> GPIO 22
