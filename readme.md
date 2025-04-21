# Marine Battery & Charger Monitor (ESP32 + Signal K)

Smart ESP32 firmware built with [SensESP](https://github.com/SignalK/SensESP) to keep tabs on your DC system and push real‑time data to your boat’s Signal K server.

---
## ⚙️ What It Does

| Function | Sensor / HW | Signal K Path |
|----------|-------------|---------------|
| New‑battery voltage | ESP32 ADC | `electrical.batteries.new.voltage` |
| Old‑battery voltage | ESP32 ADC | `electrical.batteries.old.voltage` |
| New‑battery charge/discharge current | INA219 shunt monitor | `electrical.batteries.new.current` |
| New‑battery cell temperature | DS18B20 (1‑Wire) | `electrical.batteries.new.temperature` |
| Charger heat‑sink temperature | DS18B20 (1‑Wire) | `electrical.chargers.main.temperature` |
| Charger relay state | GPIO‑driven relay | `electrical.switches.chargeRelay.state` |

> **Why these paths?** They follow the Signal K data‑model conventions for batteries, chargers, and switches, so dashboards and apps pick them up automatically.

---
## 🛠️ Hardware

* FireBeetle ESP32 (or any ESP32‑Dev board)
* **INA219** I²C current / power monitor module
* Two DS18B20 temperature probes
* External shunt resistor (configurable; default 100 mΩ)
* Single‑channel relay module (for charger enable)
* Voltage dividers (for 0‑15 V into ADC pins)

---
## 📂 Key Files

| File | Purpose |
|------|---------|
| `main.cpp` | Core logic: sensors → Signal K, relay control |
| `app_config.h` | All tweakables (Wi‑Fi, SK server, pins, thresholds, shunt value) |
| `platformio.ini` | PlatformIO build recipe + library list |

---
## 🚀 Quick Start

```bash
# 1 · Clone & open in VS Code / PlatformIO IDE
$ git clone git@github.com:jschillinger2/marine_batmon_esp.git
$ cd marine_batmon_esp

# 2 · Adjust settings
$ code app_config.h   # set Wi‑Fi SSID, SK server IP, shunt resistance, etc.

# 3 · Build & flash (USB‑connected board)
$ pio run -t upload

# 4 · Watch the serial log
$ pio device monitor
```

The device will join Wi‑Fi, auto‑discover your Signal K server (mDNS) or use the static IP you set, and begin streaming the paths above.

---
## 🔧 Config Explained (`app_config.h`)

```cpp
/******** Network ********/    kWifiSSID, kWifiPassword
/******** Signal K ******/   kSKServerIP  ("" = auto‑discover), kSKServerPort
/******** Battery ADC ****/  kAnalogInputPinNewBat / OldBat, kAnalogInputScale
/******** Relay **********/  kChargeRelayPin, kChargeOnVoltage, kChargeOffVoltage
/******** Temps **********/  kTempSensorPin, kTempReadDelay
/******** Shunt **********/  kINA219_I2C_Address,
                              kShuntResistance_Ohm,  // 0.10 Ω default
                              kCurrentReadInterval
```

---
## 📝 Wiring Notes

* **I²C** – connect INA219 SDA/SCL to ESP32 GPIO 21/22 (or whatever your board uses).
* **Shunt** – place in series with the negative lead of the new battery. Match `kShuntResistance_Ohm` to the actual resistor value.
* **Voltage dividers** – ensure the divider output never exceeds 3.3 V. Example 33 kΩ/10 kΩ covers up to ~15 V.
* **Relay** – low‑side N‑MOSFET or opto‑isolated relay board driven by `kChargeRelayPin` (GPIO 16 default).

---
## 🖥️ Dashboards & Alerts

Because the firmware uses standard Signal K paths, tools like Grafana (via InfluxDB plugin) or SignalK‑native dashboards will auto‑suggest the data. Add threshold alerts for low voltage or high current as needed.
