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

INA219_WE *ina219; // will be created in setupCurrentSensor()

// ──────────────────────────────────────────────────────────────
void setupVoltageSensors();
void setupTempSensors();
void setupCurrentSensor();
// ──────────────────────────────────────────────────────────────

void setup()
{
  SetupLogging(ESP_LOG_DEBUG);

  SensESPAppBuilder base_builder;
  auto *builder = base_builder.set_hostname("BatteryControl");

  if (strlen(kWifiSSID) > 0)
    builder->set_wifi_client(kWifiSSID, kWifiPassword);
  if (strlen(kSKServerIP) > 0)
    builder->set_sk_server(kSKServerIP, kSKServerPort);

  sensesp_app = builder->get_app();

  pinMode(kChargeRelayPin, OUTPUT);
  digitalWrite(kChargeRelayPin, LOW);

 // setupVoltageSensors();
  setupTempSensors();
 // setupCurrentSensor();

  sensesp_app->start();
}

void setupCurrentSensor()
{
  Wire.begin(); // start I²C
  ina219 = new INA219_WE(kINA219_I2C_Address);
  if (!ina219->init())
  {
    debugE("INA219 not found – current readings disabled");
    return;
  }
  ina219->setShuntSizeInOhms(kShuntResistance_Ohm);

  auto *shunt_current =
      new RepeatSensor<float>(kCurrentReadInterval, []()
                              {
                                return ina219->getCurrent_mA() / 1000.0f; // A
                              });

  shunt_current
      ->connect_to(new SKOutputFloat("electrical.shuntCurrent", ""));

  shunt_current->attach([shunt_current]
                        { debugD("Shunt current: %.3f A", shunt_current->get()); });
}

// ──────────────────────────────────────────────────────────────
void setupTempSensors()
{
  DallasTemperatureSensors *dts = new DallasTemperatureSensors(kTempSensorPin);
  uint32_t read_delay = 500;

  delay(1000); // give it time to power up and register devices
  int sensor_count = dts->get_sensor_count();

  if (sensor_count == 0) {
    debugE("No Dallas temperature sensors found on pin %d", kTempSensorPin);
    return;
  }

  debugI("Found %d Dallas temperature sensor(s):", sensor_count);


  // Measure coolant temperature
  auto* coolant_temp =
      new OneWireTemperature(dts, read_delay, "/coolantTemperature/oneWire");

  coolant_temp->connect_to(new Linear(1.0, 0.0, "/coolantTemperature/linear"))
      ->connect_to(new SKOutputFloat("propulsion.mainEngine.coolantTemperature",
                                     "/coolantTemperature/skPath"));

/*
  auto charger_temp = new OneWireTemperature(dts, read_delay, "/chargerTemperature/oneWire");
  charger_temp
      ->connect_to(new Linear(1.0, 0.0, "/chargerTemperature/linear"))
      ->connect_to(new SKOutputFloat("electrical.chargers.new.temperature", ""));

  auto newbat_temp = new OneWireTemperature(dts, read_delay, "/newBatCellTemperature/oneWire");
  newbat_temp
      ->connect_to(new Linear(1.0, 0.0, "/newBatCellTemperature/linear"))
      ->connect_to(new SKOutputFloat("electrical.batteries.new.temperature", ""));

  charger_temp->attach([charger_temp]
                       { debugD("Charger temp: %.1f °C", charger_temp->get()); });
  newbat_temp->attach([newbat_temp]
                      { debugD("New‑bat temp: %.1f °C", newbat_temp->get()); });
                       */
}

// ──────────────────────────────────────────────────────────────
void setupVoltageSensors()
{

  auto v_new = std::make_shared<AnalogInput>(kAnalogInputPinNewBat,
                                             kAnalogInputReadInterval,
                                             "voltage",
                                             kAnalogInputScale);
  auto v_old = std::make_shared<AnalogInput>(kAnalogInputPinOldBat,
                                             kAnalogInputReadInterval,
                                             "voltage",
                                             kAnalogInputScale);

  v_new->attach([v_new]
                { debugD("New‑battery V: %f", v_new->get()); });
  v_old->attach([v_old]
                { debugD("Old‑battery V: %f", v_old->get()); });

  // Voltage outputs

  v_new
      ->connect_to(new SKOutputFloat("electrical.batteries.new.voltage", ""));

  v_old
      ->connect_to(new SKOutputFloat("electrical.batteries.old.voltage", ""));

  /*  charging‑status publisher  */
  static auto charge_status = new ObservableValue<int>(0); // 0=OFF, 1=ON
  charge_status
      ->connect_to(new SKOutput<int>("electrical.switches.chargeRelay.state", ""));

  static bool relay_state = false;
  auto relay_ctl = new LambdaConsumer<float>([&](float volts)
                                             {
    if (relay_state && volts < kChargeOffVoltage) {
      relay_state = false;
      digitalWrite(kChargeRelayPin, LOW);
      charge_status->set(0);
      debugI("Relay OFF (%.2f V < %.2f V)", volts, kChargeOnVoltage);
    } else if (!relay_state && volts > kChargeOffVoltage) {
      relay_state = true;
      digitalWrite(kChargeRelayPin, HIGH);
      charge_status->set(1);
      debugI("Relay ON (%.2f V > %.2f V)", volts, kChargeOffVoltage);
    } });
  v_old->connect_to(relay_ctl);
}

// ──────────────────────────────────────────────────────────────
void loop() { event_loop()->tick(); }
// ──────────────────────────────────────────────────────────────
