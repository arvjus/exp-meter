// Copyright (c) 2022-2026, Arvid Juskaitis (arvydas.juskaitis@gmail.com)
// Version 1.1
// no bootloader, use programmer to load

#include <LiquidCrystal.h>
#include <Adafruit_TSL2561_U.h>
#include <math.h>

#define ENCODER_SW_PIN  1
#define ENCODER_CLK_PIN  2
#define ENCODER_DT_PIN  3
#define PAPER_PIN  8
#define METHOD_PIN  9
#define MEASURE_PIN  A0
#define MODE_PIN  A1
#define HIGHLIGHTS_PIN  A2
#define SHADOWS_PIN  A3

#define PAPER_FB  0
#define PAPER_RC5  1
#define METHOD_COMP  0
#define METHOD_ZONE  1
#define MODE_EXPOSURE  0
#define MODE_DENSITY  1
#define SUBMODE_EXPOSURE  0
#define SUBMODE_HIGHLIGHTS  1
#define SUBMODE_SHADOWS  2


LiquidCrystal lcd(12, 11, 10, 7, 6, 5, 4);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

int8_t mode = -1;
float lux, calibration_lux = 0, ev, ev_min, ev_min_total, ev_max, ev_max_total, calibration_ev = 0;
int8_t ev_min_count = 0, ev_max_count = 0;
unsigned long last_released = 0l;
volatile int8_t submode = SUBMODE_EXPOSURE;
volatile float brghtn_comp = 0, cntrst_comp = 0;


void setup()
{
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(PAPER_PIN, INPUT_PULLUP);
  pinMode(METHOD_PIN, INPUT_PULLUP);
  pinMode(MEASURE_PIN, INPUT_PULLUP);
  pinMode(MODE_PIN, INPUT_PULLUP);
  pinMode(HIGHLIGHTS_PIN, INPUT_PULLUP);
  pinMode(SHADOWS_PIN, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.clear();

  tsl.enableAutoRange(true);

  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), readEncoder, CHANGE);

  reset();
}

void loop()
{
  char buffer[20], buffer2[6], buffer3[6], buffer4[6];
  const float INV_LOG2 = 1.4426950408889634f;

  int8_t tmp_mode = digitalRead(MODE_PIN);
  if (tmp_mode != mode) {
    mode = tmp_mode;
    lcd.clear();
    delay(200);
  }
  
  lux = getLuminosity();
  ev = logf((float)lux / 2.5f) * INV_LOG2;

  /* Exposure meter mode ************************************************************************/
  if (mode == MODE_EXPOSURE) {
    // Get submode
    int8_t tmp_submode = submode;
    if (!digitalRead(ENCODER_SW_PIN)) {
      tmp_submode = SUBMODE_EXPOSURE;
    } else if (!digitalRead(HIGHLIGHTS_PIN)) {
      tmp_submode = SUBMODE_HIGHLIGHTS;
    } else if (!digitalRead(SHADOWS_PIN)) {
      tmp_submode = SUBMODE_SHADOWS;
    }
    if (tmp_submode != submode) {
      submode = tmp_submode;
      lcd.clear();
      delay(200);
    }

    int8_t method = digitalRead(METHOD_PIN);
    int8_t measure_flag = false, reset_flag = false;

    // Check measure button
    unsigned long now = millis();
    if (digitalRead(MEASURE_PIN)) 
      last_released = now;
    else
      measure_flag = true;
    
    if (now - last_released > 3000)
      reset_flag = true;

    /* Diaplay values */
    if (submode == SUBMODE_EXPOSURE) {
      float filter = 0;
      uint16_t exposure = 0;

      if (reset_flag) 
        reset();
      else if (measure_flag) {
        ev_min = ev_min == 0.0 ? ev : min(ev, ev_min);
        ev_max = max(ev, ev_max);
      }
      
      if (ev_min && ev_min < ev_max) {
        int8_t paper = digitalRead(PAPER_PIN);

        filter = ((paper == PAPER_RC5) ? calculate_filter_rc5(ev_max - ev_min) : calculate_filter_fb(ev_max - ev_min)) + cntrst_comp;
        exposure = (paper == PAPER_RC5) ? calculate_exposure_rc5(filter, ev_min) : calculate_exposure_fb(filter, ev_min);
        exposure *= pow(2, -brghtn_comp);
      }

      sprintf(buffer, "E%5s%5s%5s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_min, 5, 2, buffer3), dtostrf(ev_max, 5, 2, buffer4));
      lcd.setCursor(0, 0);
      lcd.print(buffer);

      sprintf(buffer, "%6s %3ds F%3s ", dtostrf(ev_max - ev_min, 6, 2, buffer2), exposure, dtostrf(filter, 3, 1, buffer3));
      lcd.setCursor(0, 1);
      lcd.print(buffer);

    /* Measure highlights */
    } else if (submode == SUBMODE_HIGHLIGHTS) {
        if (reset_flag) {
          ev_min = 0.0;
          ev_min_total = 0.0;
          ev_min_count = 0;
          brghtn_comp = 0.0;
        } else if (measure_flag) {
          ev_min_total += ev;
          ev_min_count ++;
          ev_min = ev_min_total / ev_min_count;
        }

        sprintf(buffer, "H%5s%5s %4s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_min, 5, 2, buffer3), dtostrf(brghtn_comp, 4, 1, buffer4));
        lcd.setCursor(0, 0);
        lcd.print(buffer);
        
    /* Measure shadows */
    } else if (submode == SUBMODE_SHADOWS) {
        if (reset_flag) {
          ev_max = 0.0;
          ev_max_total = 0.0;
          ev_max_count = 0;
          cntrst_comp = 0.0;
        } else if (measure_flag) {
          ev_max_total += ev;
          ev_max_count ++;
          ev_max = ev_max_total / ev_max_count;
        }

        sprintf(buffer, "S%5s%5s %4s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_max, 5, 2, buffer3), dtostrf(cntrst_comp, 4, 1, buffer4));
        lcd.setCursor(0, 0);
        lcd.print(buffer);
    }

    // Delay after value update
    if (measure_flag)
      delay(200);

  /* Density meter mode ************************************************************************/
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
}

void reset() 
{
  lux = 0;
  ev = 0.0;
  ev_min = 0.0;
  ev_min_total = 0.0;
  ev_min_count = 0;
  ev_max = 0.0;
  ev_max_total = 0.0;
  ev_max_count = 0;
  brghtn_comp = 0.0;
  cntrst_comp = 0.0;
  last_released = 0l;

  lcd.clear();
}

float getLuminosity()
{
    uint16_t broadband, infrared;

    // Fast measurement
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);
    delay(110);
    tsl.getLuminosity(&broadband, &infrared);

    if (broadband >= 8)
        return (float)broadband;

    // Low light: longer integration
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);
    delay(420);
    tsl.getLuminosity(&broadband, &infrared);

    return broadband / 4.0f;
}

void readEncoder() {
  static unsigned long last_interrupt = 0;
  unsigned long now = millis();
  if (now - last_interrupt < 50)
    return;

  // PIND & 0b00000100 isolates Pin 2 (Bit 2)
  // PIND & 0b00001000 isolates Pin 3 (Bit 3)
  bool clkState = PIND & _BV(PD2); 
  bool dtState  = PIND & _BV(PD3);

  if (submode == SUBMODE_HIGHLIGHTS) {
    if (clkState != dtState && brghtn_comp < 1.5f) {
      brghtn_comp += 0.5f;
    } else if (clkState == dtState && brghtn_comp > -1.5f) {
      brghtn_comp -= 0.5f;
    }
  } else if (submode == SUBMODE_SHADOWS) {
    if (clkState != dtState && cntrst_comp < 1.5f) {
      cntrst_comp += 0.5f;
    } else if (clkState == dtState && cntrst_comp > -1.5f) {
      cntrst_comp -= 0.5f;
    }
  }
  
  last_interrupt = now;
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
