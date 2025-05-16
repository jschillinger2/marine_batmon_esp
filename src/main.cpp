// main.cpp – Signal K Battery Control (no config‑paths)
// -------------------------------------------------------------
// Updated 20 Apr 2025
//   • Dropped all per‑device config paths (“” used instead).
// -------------------------------------------------------------

#include <memory>
#include <cstring>
#include <vector>
#include <algorithm>

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

// Convert voltage to state of charge
float interpolate(float x, const std::vector<float> &xPoints, const std::vector<float> &yPoints)
{
  auto it = std::lower_bound(xPoints.begin(), xPoints.end(), x);
  int idx = std::distance(xPoints.begin(), it);

  if (idx == 0)
  {
    return yPoints.front();
  }
  else if (idx == xPoints.size())
  {
    return yPoints.back();
  }

  // Interpolate between points
  float x0 = xPoints[idx - 1], y0 = yPoints[idx - 1];
  float x1 = xPoints[idx], y1 = yPoints[idx];
  return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
}

float convertVoltageToSOC(float voltage, bool isLeadAcid)
{
  std::vector<float> voltagesLeadAcid = {1.0f, 1.0f, 12.2f, 12.4f, 12.6f};
  std::vector<float> socLeadAcid = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

  std::vector<float> voltagesLiFePo4 = {1.0f, 1.0f, 13.2f, 13.4f, 13.6f};
  std::vector<float> socLiFePo4 = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

  if (isLeadAcid)
  {
    return interpolate(voltage, voltagesLeadAcid, socLeadAcid);
  }
  else
  {
    return interpolate(voltage, voltagesLiFePo4, socLiFePo4);
  }
}

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

  setupVoltageSensors();
  setupTempSensors();
  setupCurrentSensor();

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

  // Count all connected temperature sensors
  OWDevAddr addr;
  int sensor_count = 0;
  while (dts->get_next_address(&addr))
  {
    sensor_count++;
  }
  debugI("Number of temperature sensors found: %d", sensor_count);

  // Measure coolant temperature
  auto *coolant_temp =
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

  /* state of charge publishers */
  static auto new_state_charge = new ObservableValue<float>(0); // 0=empty, 1=full
  new_state_charge
      ->connect_to(new SKOutput<float>("electrical.batteries.new.chargestate", ""));
  static auto old_state_charge = new ObservableValue<float>(0); // 0=empty, 1=full
  old_state_charge
      ->connect_to(new SKOutput<float>("electrical.batteries.old.chargestate", ""));

  static bool relay_state = false;
  auto relay_ctl = new LambdaConsumer<float>([&](float volts)
                                             {
                                               // charge relay
                                               if (relay_state && volts < kChargeOffVoltage)
                                               {
                                                 relay_state = false;
                                                 digitalWrite(kChargeRelayPin, LOW);
                                                 charge_status->set(0);
                                                 debugI("Relay OFF (%.2f V < %.2f V)", volts, kChargeOffVoltage);
                                               }
                                               else if (!relay_state && volts > kChargeOnVoltage)
                                               {
                                                 relay_state = true;
                                                 digitalWrite(kChargeRelayPin, HIGH);
                                                 charge_status->set(1);
                                                 debugI("Relay ON (%.2f V > %.2f V)", volts, kChargeOnVoltage);
                                               }

                                               // state of charge
                                               old_state_charge->set(convertVoltageToSOC(volts, true)); // Assume lead acid
                                             });
  v_old->connect_to(relay_ctl);

  auto new_state_ctrl = new LambdaConsumer<float>([&](float volts)
                                                  {
                                                    // state of charge
                                                    new_state_charge->set(convertVoltageToSOC(volts, false)); // Assume LiFePo4
                                                  });

  v_new->connect_to(new_state_ctrl);
}

// ──────────────────────────────────────────────────────────────
void loop() { event_loop()->tick(); }
// ──────────────────────────────────────────────────────────────
