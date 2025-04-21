// main.cpp – Signal K Battery Control (no config‑paths)
// -------------------------------------------------------------
// Updated 20 Apr 2025
//   • Dropped all per‑device config paths (“” used instead).
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

#include <Wire.h>
#include <INA219_WE.h>

#include "app_config.h"



using namespace sensesp;
using namespace sensesp::onewire;

INA219_WE* ina219;         // will be created in setupCurrentSensor()


// ──────────────────────────────────────────────────────────────
void setupVoltageSensors();
void setupTempSensors();
void setupCurrentSensor();
// ──────────────────────────────────────────────────────────────

void setup() {
  SetupLogging(ESP_LOG_DEBUG);

  SensESPAppBuilder base_builder;
  auto* builder = base_builder.set_hostname("BatteryControl");

  if (strlen(kWifiSSID) > 0) builder->set_wifi_client(kWifiSSID, kWifiPassword);
  if (strlen(kSKServerIP) > 0) builder->set_sk_server(kSKServerIP, kSKServerPort);

  sensesp_app = builder->get_app();

  pinMode(kChargeRelayPin, OUTPUT);
  digitalWrite(kChargeRelayPin, LOW);

  setupVoltageSensors();
  setupTempSensors();
  setupCurrentSensor();

  while (true) loop();
}

void setupCurrentSensor() {
  Wire.begin();                                      // start I²C
  ina219 = new INA219_WE(kINA219_I2C_Address);
  if (!ina219->init()) {
    debugE("INA219 not found – current readings disabled");
    return;
  }
  ina219->setShuntSizeInOhms(kShuntResistance_Ohm);

  auto* shunt_current =
      new RepeatSensor<float>(kCurrentReadInterval, []() {
        return ina219->getCurrent_mA() / 1000.0f;    // A
      });

  shunt_current
      ->connect_to(new SKOutputFloat("electrical.shuntCurrent", ""));
}


// ──────────────────────────────────────────────────────────────
void setupTempSensors() {
  DallasTemperatureSensors* dts = new DallasTemperatureSensors(kTempSensorPin);
  uint32_t read_delay = 500;

  auto charger_temp = new OneWireTemperature(dts, read_delay, "/chargerTemperature/oneWire");
  charger_temp
      ->connect_to(new Linear(1.0, 0.0, "/chargerTemperature/linear"))
      ->connect_to(new SKOutputFloat("electrical.chargers.new.temperature", ""));

  auto newbat_temp = new OneWireTemperature(dts, read_delay, "/newBatCellTemperature/oneWire");
  newbat_temp
      ->connect_to(new Linear(1.0, 0.0, "/newBatCellTemperature/linear"))
      ->connect_to(new SKOutputFloat("electrical.batteries.new.temperature", ""));
}

// ──────────────────────────────────────────────────────────────
void setupVoltageSensors() {
  auto v_new = std::make_shared<AnalogInput>(kAnalogInputPinNewBat,
                                             kAnalogInputReadInterval,
                                             "voltage",
                                             kAnalogInputScale);
  auto v_old = std::make_shared<AnalogInput>(kAnalogInputPinOldBat,
                                             kAnalogInputReadInterval,
                                             "voltage",
                                             kAnalogInputScale);

  v_new->attach([v_new] { debugD("New‑battery V: %f", v_new->get()); });
  v_old->attach([v_old] { debugD("Old‑battery V: %f", v_old->get()); });

  /*  charging‑status publisher  */
  static auto charge_status = new ObservableValue<int>(0);   // 0=OFF, 1=ON
  charge_status
      ->connect_to(new SKOutput<int>("electrical.switches.chargeRelay.state", ""));

  static bool relay_state = false;
  auto relay_ctl = new LambdaConsumer<float>([&](float volts) {
    if (!relay_state && volts < kChargeOnVoltage) {
      relay_state = true;
      digitalWrite(kChargeRelayPin, HIGH);
      charge_status->set(1);
      debugI("Relay ON (%.2f V < %.2f V)", volts, kChargeOnVoltage);
    } else if (relay_state && volts > kChargeOffVoltage) {
      relay_state = false;
      digitalWrite(kChargeRelayPin, LOW);
      charge_status->set(0);
      debugI("Relay OFF (%.2f V > %.2f V)", volts, kChargeOffVoltage);
    }
  });
  v_old->connect_to(relay_ctl);

  // Voltage outputs
  {
    auto meta = std::make_shared<SKMetadata>("V", "Analog input voltage – New Battery");
    v_new->connect_to(std::make_shared<SKOutput<float>>(
        "electrical.batteries.new.voltage", "", meta));
  }
  {
    auto meta = std::make_shared<SKMetadata>("V", "Analog input voltage – Old Battery");
    v_old->connect_to(std::make_shared<SKOutput<float>>(
        "electrical.batteries.old.voltage", "", meta));
  }
}

// ──────────────────────────────────────────────────────────────
void loop() { event_loop()->tick(); }
// ──────────────────────────────────────────────────────────────
