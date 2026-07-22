// Copyright (c) 2022-2026, Arvid Juskaitis (arvydas.juskaitis@gmail.com)
// no bootloader, use programmer to load

#include <LiquidCrystal.h>
#include <Adafruit_TSL2561_U.h>
#include <math.h>

#define PAPER_PIN  1
#define MEASURE_PIN  6
#define MODE_PIN  7
#define BRGHTN_COMP_PIN  8
#define CNTRST_COMP_PIN  9
#define COMP_VALUE_PIN  A3

#define MODE_EXPOSURE  0
#define MODE_DENSITY  1
#define PAPER_RC5  0
#define PAPER_FB  1


LiquidCrystal lcd(12, 11, 10, 5, 4, 3, 2);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

int8_t prev_mode = -1;
float lux, calibration_lux = 0, ev, ev_min, ev_max, calibration_ev = 0, brghtn_comp = 0, cntrst_comp = 0;
unsigned long last_measure = 0l;


void setup()
{
  pinMode(PAPER_PIN, INPUT_PULLUP);
  pinMode(MEASURE_PIN, INPUT_PULLUP);
  pinMode(MODE_PIN, INPUT_PULLUP);
  pinMode(BRGHTN_COMP_PIN, INPUT_PULLUP);
  pinMode(CNTRST_COMP_PIN, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.clear();

  tsl.enableAutoRange(true);

  reset();
}

void loop()
{
  char buffer[20], buffer2[6], buffer3[6], buffer4[6];
  const float INV_LOG2 = 1.4426950408889634f;

  lux = getLuminosity();
  ev = logf((float)lux / 2.5f) * INV_LOG2;

  int8_t mode = digitalRead(MODE_PIN);
  if (mode != prev_mode) {
    prev_mode = mode;
    lcd.clear();
    delay(300);
  }

  if (mode == MODE_EXPOSURE) {
    if (!digitalRead(BRGHTN_COMP_PIN) || !digitalRead(CNTRST_COMP_PIN)) {
      lcd.clear();
      if (!digitalRead(BRGHTN_COMP_PIN)) {
        brghtn_comp = readCompValue();
        sprintf(buffer, "Brightness %3s", dtostrf(brghtn_comp, 3, 1, buffer2));
      }
      else if (!digitalRead(CNTRST_COMP_PIN)) {
        cntrst_comp = readCompValue();
        sprintf(buffer, "Contrast %3s", dtostrf(cntrst_comp, 3, 1, buffer2));
      }

      lcd.setCursor(0, 0);
      lcd.print(buffer);
      delay(500);
    }

    unsigned long now = millis();
    if (!digitalRead(MEASURE_PIN)) {
      ev_min = ev_min == 0.0 ? ev : min(ev, ev_min);
      ev_max = max(ev, ev_max);
    } else
      last_measure = now;

    if (now - last_measure > 2000)
      reset();

    float filter = 0;
    uint16_t exposure = 0;

    if (ev_min) {
      int8_t paper = digitalRead(PAPER_PIN);

      filter = (paper == PAPER_RC5) ? calculate_filter_rc5(ev_max - ev_min) : calculate_filter_fb(ev_max - ev_min) + cntrst_comp;
      exposure = (paper == PAPER_RC5) ? calculate_exposure_rc5(filter, ev_min) : calculate_exposure_fb(filter, ev_min);
      exposure *= pow(2, -brghtn_comp);
    }

    sprintf(buffer, "E%5s %4s %4s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_min, 4, 1, buffer3), dtostrf(ev_max, 4, 1, buffer4));
    lcd.setCursor(0, 0);
    lcd.print(buffer);

    sprintf(buffer, "%6s %3ds F%3s ", dtostrf(ev_max - ev_min, 6, 2, buffer2), exposure, dtostrf(filter, 3, 1, buffer3));
    lcd.setCursor(0, 1);
    lcd.print(buffer);

  } else if (mode == MODE_DENSITY) {
    // calibration
    if (!digitalRead(MEASURE_PIN)) {
      calibration_lux = lux;
      calibration_ev = ev;
    }

    float d = log(calibration_lux / lux) / log(10);
    sprintf(buffer, "D%6s %5s", dtostrf(d, 6, 3, buffer2), dtostrf(calibration_ev - ev, 5, 2, buffer3));
    lcd.setCursor(0, 0);
    lcd.print(buffer);
  }
  delay(100);
}

void reset() 
{
  lux = 0;
  ev = 0.0;
  ev_min = 0.0;
  ev_max = 0.0;
  brghtn_comp = 0.0;
  cntrst_comp = 0.0;
  last_measure = 0l;

  lcd.clear();
}

float getLuminosity()
{
    uint16_t broadband, infrared;

    // Fast measurement
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);
    delay(110);
    tsl.getLuminosity(&broadband, &infrared);

    if (broadband >= 16)
        return (float)broadband;

    // Low light: longer integration
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);
    delay(420);
    tsl.getLuminosity(&broadband, &infrared);

    return broadband / 4.0f;
}

float readCompValue()
{
  uint16_t value = analogRead(COMP_VALUE_PIN);
  if (value < 1) return -1.0;
  else if (value < 1) return -1.0;
  else if (value < 30) return -0.5;
  else if (value < 100) return 0;
  else if (value < 900) return 0.5;
  else return 1.0;
}

// To calculate filter, we use quadratic function:
// filter = a + b * ev + c * ev * ev;
// quadratic in Horner's form:
// filter = (c * ev + b) * ev + a;

// 0.152528045  -2.695569866  8.743406703
float calculate_filter_rc5(float ev)
{
  float filter = (0.152528045f * ev + -2.695569866f) * ev + 8.743406703f;

  filter = roundf(filter * 2.0f) * 0.5f;

  if (filter < 0.0f) return 0.0f;
  if (filter > 5.0f) return 5.0f;

  return filter;
}

// 0.082128023  -2.105486282  7.656978866
float calculate_filter_fb(float ev)
{
    float filter = (0.082128023f * ev + -2.105486282f) * ev + 7.656978866f;

    filter = roundf(filter * 2.0f) * 0.5f;

    if (filter < 0.0f) return 0.0f;
    if (filter > 5.0f) return 5.0f;

    return filter;
}

// To clalculate exposure, 
// 3.07 is the EV value used in calibration for getting these exp times- measuring without any filter

uint16_t calculate_exposure_rc5(float filter, float ev_min)
{
  static float exp_times[] = { 
    2.3101f, 2.3500f,   // 0
    2.3898f, 2.5491f,   // 1
    2.7084f, 2.7881f,   // 2
    2.8678f, 2.9474f,   // 3
    5.5762f, 5.8152f,   // 4
    6.0542f };          // 5
  return round(exp_times[int(filter * 2)] * pow(2, (3.07 - ev_min)));
}

uint16_t calculate_exposure_fb(float filter, float ev_min)
{
  static float exp_times[] = { 
    2.3898f, 2.3898f,   // 0
    2.3898f, 2.3898f,   // 1
    2.3898f, 2.5491f,   // 2
    2.7084f, 2.8678f,   // 3
    4.7796f, 4.9588f,   // 4
    5.1779f };          // 5
  return round(exp_times[int(filter * 2)] * pow(2, (3.07 - ev_min)));
}
