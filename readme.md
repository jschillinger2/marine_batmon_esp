# Marine Battery Monitor (ESP32 + SignalK)

A smart marine battery monitor for ESP32 using [SensESP](https://github.com/SignalK/SensESP), designed to:

- Monitor voltage of two batteries (new + old)
- Measure temperatures via 1-Wire sensors
- Automatically control a charger relay for the old battery using hysteresis logic
- Send all data to a SignalK server over Wi-Fi (with auto-discovery support)

---

## 🔧 Features

- ⚡ Analog voltage sensing for two batteries
- 🌡️ Temperature sensing via Dallas 1-Wire
- 🔁 Automatic charger control (relay) with configurable voltage thresholds
- 📡 Wi-Fi & SignalK connectivity (configurable in `app_config.h`)
- 🪛 Fully open source, editable via PlatformIO

---

## 📂 File Structure

- `main.cpp`: main firmware logic
- `app_config.h`: user-editable settings (Wi-Fi, pins, thresholds)
- `platformio.ini`: PlatformIO build config

---

## 🚀 Getting Started

### 1. Clone the repo

```bash
git clone git@github.com:jschillinger2/marine_batmon_esp.git
cd marine_batmon_esp
