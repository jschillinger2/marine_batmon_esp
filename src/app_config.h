#pragma once
// app_config.h – Editable device properties
// -------------------------------------------------------------
// Modify the constants below, then re‑compile & flash.
// -------------------------------------------------------------

/************* Network *************/
constexpr const char* kWifiSSID       = "xx";   // Wi‑Fi SSID
constexpr const char* kWifiPassword   = "xx";   // Wi‑Fi password

/********* Signal K Server *********/
// Leave kSKServerIP empty ("") for auto‑discovery via mDNS.
constexpr const char*  kSKServerIP    = "";        // e.g. "10.10.7.16"
constexpr const uint16_t kSKServerPort = 3000;     // usual SK port

/*********** Battery Inputs ***********/
// ADC GPIO numbers
constexpr uint8_t kAnalogInputPinNewBat = 35;  // new battery voltage
constexpr uint8_t kAnalogInputPinOldBat = 39;  // old battery voltage

constexpr unsigned int kAnalogInputReadInterval = 500; // ms between samples
// Scale factor to convert ADC count to volts
constexpr float kAnalogInputScale = 1.0f / 4095.0f * 3.3f * 12.2f;

/************* Charger Relay *************/
constexpr uint8_t kChargeRelayPin   = 16;    // GPIO driving the relay
constexpr float   kChargeOnVoltage  = 12.8f; // V – turn charger ON below
constexpr float   kChargeOffVoltage = 14.0f; // V – turn charger OFF above

/************* Temp Sensors **************/
constexpr uint8_t kTempSensorPin = 4;        // 1‑Wire data pin
constexpr uint16_t kTempReadDelay = 500;     // ms between temp samples

/************** Shunt / Current Sensor **************/
constexpr uint8_t  kINA219_I2C_Address   = 0x40;   // default INA219 addr
constexpr float    kShuntResistance_mOhm = 100.0f; // change to suit your shunt
constexpr uint16_t kCurrentReadInterval  = 500;    // ms between current reads
constexpr float kShuntResistance_Ohm = 0.10f;   // 0.10 Ω = 100 mΩ
