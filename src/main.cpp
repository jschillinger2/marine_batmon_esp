// main.cpp – Signal K Battery Control
// -------------------------------------------------------------
// Updates (Apr 20 2025)
//   • Wi‑Fi creds & SK server address exposed as constants.
//   • Auto‑discovery used if kSKServerIP is empty.
//   • Old‑battery hysteresis relay control.
//   • Compile‑time fixes for SensESPAppBuilder chaining.
// -------------------------------------------------------------

#include <memory>
#include <cstring>

#include "sensesp.h"
#include "sensesp/sensors/analog_input.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp_app_builder.h"

#include "sensesp/transforms/linear.h"
#include "sensesp_onewire/onewire_temperature.h"

#include "app_config.h"  // <‑‑ all user‑editable properties


using namespace sensesp;
using namespace sensesp::onewire;

// ──────────────────────────────────────────────────────────────
// Forward declarations
// ──────────────────────────────────────────────────────────────
void setupVoltageSensors();
void setupTempSensors();

// ──────────────────────────────────────────────────────────────
// setup()
// ──────────────────────────────────────────────────────────────
void setup() {
  SetupLogging(ESP_LOG_DEBUG);

  // Base builder
  SensESPAppBuilder base_builder;

  // First call returns a *pointer*; store it so subsequent calls use "->".
  SensESPAppBuilder* builder = base_builder.set_hostname("BatteryControl");

  // Wi‑Fi credentials (skip if SSID empty)
  if (strlen(kWifiSSID) > 0) {
    builder->set_wifi_client(kWifiSSID, kWifiPassword);
  }

  // Optional fixed SK server; fall back to auto‑discover when empty
  if (strlen(kSKServerIP) > 0) {
    builder->set_sk_server(kSKServerIP, kSKServerPort);
  }

  // Obtain the global SensESP app instance
  sensesp_app = builder->get_app();

  // Hardware setup
  pinMode(kChargeRelayPin, OUTPUT);
  digitalWrite(kChargeRelayPin, LOW); // assume relay OFF at boot

  setupVoltageSensors();
  setupTempSensors();

  // Continuous loop
  while (true) {
    loop();
  }
}

// ──────────────────────────────────────────────────────────────
// Temperature sensors (unchanged)
// ──────────────────────────────────────────────────────────────
void setupTempSensors() {
  DallasTemperatureSensors* dts = new DallasTemperatureSensors(kTempSensorPin);
  uint32_t read_delay = 500; // ms

  // Charger temperature
  {
    auto charger_temp = new OneWireTemperature(dts, read_delay, "/chargerTemperature/oneWire");
    auto charger_cal  = new Linear(1.0, 0.0, "/chargerTemperature/linear");
    auto charger_out  = new SKOutputFloat("electrical.chargerTemperature", "/chargerTemperature/skPath");
    charger_temp->connect_to(charger_cal)->connect_to(charger_out);
  }

  // New‑battery cell temperature
  {
    auto newbat_temp = new OneWireTemperature(dts, read_delay, "/newBatCellTemperature/oneWire");
    auto newbat_cal  = new Linear(1.0, 0.0, "/newBatCellTemperature/linear");
    auto newbat_out  = new SKOutputFloat("electrical.newBatCellTemperature", "/newBatCellTemperature/skPath");
    newbat_temp->connect_to(newbat_cal)->connect_to(newbat_out);
  }
}

// ──────────────────────────────────────────────────────────────
// Voltage sensors + relay control
// ──────────────────────────────────────────────────────────────
void setupVoltageSensors() {
  // Analog inputs
  auto analog_input_new = std::make_shared<AnalogInput>(
      kAnalogInputPinNewBat, kAnalogInputReadInterval, "voltage", kAnalogInputScale);
  auto analog_input_old = std::make_shared<AnalogInput>(
      kAnalogInputPinOldBat, kAnalogInputReadInterval, "voltage", kAnalogInputScale);

  // Debug log
  analog_input_new->attach([analog_input_new]() {
    debugD("New‑battery V: %f", analog_input_new->get());
  });
  analog_input_old->attach([analog_input_old]() {
    debugD("Old‑battery V: %f", analog_input_old->get());
  });

  // Relay hysteresis controller for old battery
  static bool relay_state = false; // false = OFF
  auto relay_control = new LambdaConsumer<float>([&](float volts) {
    if (!relay_state && volts < kChargeOnVoltage) {
      relay_state = true;
      digitalWrite(kChargeRelayPin, HIGH);
      debugI("Relay ON (%.2f V < %.2f)", volts, kChargeOnVoltage);
    } else if (relay_state && volts > kChargeOffVoltage) {
      relay_state = false;
      digitalWrite(kChargeRelayPin, LOW);
      debugI("Relay OFF (%.2f V > %.2f)", volts, kChargeOffVoltage);
    }
  });
  analog_input_old->connect_to(relay_control);

  // Signal K outputs
  {
    auto meta_new = std::make_shared<SKMetadata>("V", "Analog input voltage – New Battery");
    auto sk_new   = std::make_shared<SKOutput<float>>("sensors.analog_input.voltageNewBat", "/Sensors/Analog Input/VoltageNewBat", meta_new);
    analog_input_new->connect_to(sk_new);
  }
  {
    auto meta_old = std::make_shared<SKMetadata>("V", "Analog input voltage – Old Battery");
    auto sk_old   = std::make_shared<SKOutput<float>>("sensors.analog_input.voltageOldBat", "/Sensors/Analog Input/VoltageOldBat", meta_old);
    analog_input_old->connect_to(sk_old);
  }
}

// ──────────────────────────────────────────────────────────────
// loop()
// ──────────────────────────────────────────────────────────────
void loop() {
  event_loop()->tick();
}
