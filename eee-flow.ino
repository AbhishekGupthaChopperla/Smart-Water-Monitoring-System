/*
  Smart Water Controller (ESP32) - INVERTED PUMP LOGIC VERSION
  Sensors:
    - DS18B20 Temperature (GPIO4)
    - Turbidity SEN0175 (GPIO34)
    - pH (E201 Po) (GPIO33)
    - TDS Gravity V1.0 using GravityTDS_ESP32 (GPIO35)

  Outputs:
    - Relay-controlled Pump (GPIO13)
    - Green LED → GPIO25  (water safe)
    - Red LED → GPIO26    (water unsafe)
    - Buzzer → GPIO14
    - LCD16x2 I2C (SDA=21, SCL=22)

  NOTE: This version includes a flag INVERT_PUMP_LOGIC.
        If INVERT_PUMP_LOGIC = true, the pump state is inverted
        relative to the normal "pump ON when safe" behavior.
*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include "GravityTDS.h"

// ---------------- PIN DEFINITIONS ----------------
#define DS18B20_PIN 4
#define TURB_PIN    34
#define PH_PIN      33
#define TDS_PIN     35

#define RELAY_PIN   27  // relay IN
#define GREEN_LED   25
#define RED_LED     26
#define BUZZER_PIN  14

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// TDS
GravityTDS gravityTds;

// ADC constants
const float ADC_REF = 3.3;
const float ADC_MAX = 4095.0;

// DS18B20
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// thresholds
const float PH_MIN = 6.5;
const float PH_MAX = 11.0;
const float TDS_MAX = 500.0;     // ppm
const float TEMP_MIN = 15.0;     // °C
const float TEMP_MAX = 45.0;     // °C
const float NTU_LIMIT = 5.0;     // NTU

// TDS smoothing
float smoothTDS = 0.0;
const float ALPHA = 0.12;

// Relay logic
const bool RELAY_ACTIVE_LOW = false; // false => active HIGH (HIGH = ON)

// INVERT PUMP LOGIC FLAG
// If true, pump ON/OFF is inverted (useful when wiring/relay semantics require inversion)
const bool INVERT_PUMP_LOGIC = true; // <--- you asked to invert pump logic

//-----------------------------------------------
void setPump(bool on) {
  if (RELAY_ACTIVE_LOW)
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  else
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
}
//-----------------------------------------------

// read averaged ADC helper
float readAveragedADC(int pin, int samples = 20, int delayMs = 4) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(delayMs);
  }
  return (float)sum / (float)samples;
}

// Convert turbidity voltage to NTU
float convertTurbidityVoltageToNTU(float v) {
  float NTU = -1120.4 * v * v + 5742.3 * v - 4352.9;
  if (NTU < 0) NTU = 0;
  return NTU;
}

// Convert pH voltage to approximate pH (needs calibration)
float convertPhVoltageToPH(float v) {
  float pH = 7.0 + ((2.5 - v) * 3.0);  // simple linear formula
  return pH;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // ADC configuration
  analogReadResolution(12);
  analogSetPinAttenuation(TDS_PIN, ADC_11db);
  analogSetPinAttenuation(PH_PIN, ADC_11db);
  analogSetPinAttenuation(TURB_PIN, ADC_11db);

  sensors.begin();

  lcd.init();
  lcd.backlight();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // start with pump OFF (apply inversion later)
  setPump(false);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // TDS init
  gravityTds.setPin(TDS_PIN);
  gravityTds.setAref(3.3);
  gravityTds.setAdcRange(4095);
  gravityTds.begin();

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Smart Water Sys");
  lcd.setCursor(0,1);
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();

  Serial.println("Smart Water Controller (INVERT PUMP LOGIC MODE)");
  Serial.println("TDS calibration: ENTER / CAL:707 / EXIT");
}

// main loop
void loop() {
  // 1. Temperature
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC < -20 || tempC > 125) tempC = 25.0; // fallback

  // 2. Turbidity
  float turbRaw  = readAveragedADC(TURB_PIN, 10, 3);
  float turbVolt = turbRaw * (ADC_REF / ADC_MAX);
  float NTU      = convertTurbidityVoltageToNTU(turbVolt);

  // 3. pH
  float phRaw  = readAveragedADC(PH_PIN, 20, 3);
  float phVolt = phRaw * (ADC_REF / ADC_MAX);
  float pH     = convertPhVoltageToPH(phVolt);

  // 4. TDS
  gravityTds.setTemperature(tempC);
  gravityTds.update();
  float TDS = gravityTds.getTdsValue();

  // smoothing TDS
  if (smoothTDS == 0) smoothTDS = TDS;
  smoothTDS = ALPHA * TDS + (1 - ALPHA) * smoothTDS;

  // 5. Water safety check
  bool badWater = false;
  if (tempC < TEMP_MIN || tempC > TEMP_MAX) badWater = true;
  if (pH < PH_MIN || pH > PH_MAX) badWater = true;
  if (smoothTDS > TDS_MAX) badWater = true;
  if (NTU > NTU_LIMIT) badWater = true;

  // 6. Determine desired pump state (default: ON when safe)
  bool pumpShouldBeOn = !badWater; // true if safe

  // apply inversion flag if requested
  if (INVERT_PUMP_LOGIC) pumpShouldBeOn = !pumpShouldBeOn;

  // enforce pump state
  setPump(pumpShouldBeOn);

  // LED / buzzer logic (reflect badWater, not inverted)
  if (badWater) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(80);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // LCD update
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print((int)tempC);
  lcd.print("C N:");
  lcd.print((int)NTU);

  lcd.setCursor(0,1);
  lcd.print("pH:");
  lcd.print(pH,1);
  lcd.print(" T:");
  lcd.print((int)smoothTDS);

  // Serial debug
  Serial.print("TEMP: "); Serial.print(tempC);
  Serial.print(" | NTU: "); Serial.print(NTU);
  Serial.print(" | pH: "); Serial.print(pH);
  Serial.print(" | TDS: "); Serial.print(smoothTDS);
  Serial.print(" | BAD WATER: "); Serial.print(badWater ? "YES" : "NO");
  Serial.print(" | PUMP_ON: "); Serial.println(pumpShouldBeOn ? "YES" : "NO");

  delay(400);
}
