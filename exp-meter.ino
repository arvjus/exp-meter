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
float lux, lux_calibration = 0, ev, ev_highlights, ev_highlights_total, ev_shadows, ev_shadows_total, ev_calibration = 0;
int8_t ev_highlights_count = 0, ev_shadows_count = 0;
unsigned long last_released = 0l;
volatile int8_t method = -1, submode = SUBMODE_EXPOSURE;
volatile float comp_brightness = 0, comp_contrast = 0;
volatile int8_t zone_highlights = 8, zone_shadows = 1;

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

  int8_t tmp_method = digitalRead(METHOD_PIN);
  if (tmp_method != method) {
    method = tmp_method;
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
        ev_highlights = ev_highlights == 0.0 ? ev : min(ev, ev_highlights);
        ev_shadows = max(ev, ev_shadows);
      }
      
      const float ev_contrast = ev_shadows - ev_highlights;
      if (ev_highlights && ev_contrast > 0.0f) {
        int8_t paper = digitalRead(PAPER_PIN);
        if (method == METHOD_ZONE && zone_highlights - zone_shadows >= 1) {
          const float zone_step = ev_contrast / (zone_highlights - zone_shadows);
          const uint8_t target_zone_shadows = 1, target_zone_highlights = 8;
          const float target_ev_highlights = ev_highlights - (target_zone_highlights - zone_highlights) * zone_step;
          const float target_ev_shadows = ev_shadows + (zone_shadows - target_zone_shadows) * zone_step;
          filter = ((paper == PAPER_RC5) ? calculate_filter_rc5(target_ev_shadows - target_ev_highlights) : calculate_filter_fb(target_ev_shadows - target_ev_highlights));
          exposure = (paper == PAPER_RC5) ? calculate_exposure_rc5(filter, target_ev_highlights) : calculate_exposure_fb(filter, target_ev_highlights);
        } else if (method == METHOD_COMP) {
          filter = ((paper == PAPER_RC5) ? calculate_filter_rc5(ev_contrast) : calculate_filter_fb(ev_contrast)) + comp_contrast;
          exposure = (paper == PAPER_RC5) ? calculate_exposure_rc5(filter, ev_highlights) : calculate_exposure_fb(filter, ev_highlights);
          exposure *= pow(2, -comp_brightness);
        }
      }

      sprintf(buffer, "E%5s%5s%5s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_highlights, 5, 2, buffer3), dtostrf(ev_shadows, 5, 2, buffer4));
      lcd.setCursor(0, 0);
      lcd.print(buffer);

      sprintf(buffer, "%6s %3ds F%3s ", dtostrf(ev_contrast, 6, 2, buffer2), exposure, dtostrf(filter, 3, 1, buffer3));
      lcd.setCursor(0, 1);
      lcd.print(buffer);

    /* Measure highlights */
    } else if (submode == SUBMODE_HIGHLIGHTS) {
        if (reset_flag) {
          ev_highlights = 0.0;
          ev_highlights_total = 0.0;
          ev_highlights_count = 0;
          comp_brightness = 0.0;
          zone_highlights = 8;
        } else if (measure_flag) {
          ev_highlights_total += ev;
          ev_highlights_count ++;
          ev_highlights = ev_highlights_total / ev_highlights_count;
        }

        if (method == METHOD_ZONE) 
          sprintf(buffer, "H%5s%5s %-4s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_highlights, 5, 2, buffer3), display_zone(zone_highlights, buffer4));
        else if (method == METHOD_COMP) 
          sprintf(buffer, "H%5s%5s %4s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_highlights, 5, 2, buffer3), dtostrf(comp_brightness, 4, 1, buffer4));
        lcd.setCursor(0, 0);
        lcd.print(buffer);
        
    /* Measure shadows */
    } else if (submode == SUBMODE_SHADOWS) {
        if (reset_flag) {
          ev_shadows = 0.0;
          ev_shadows_total = 0.0;
          ev_shadows_count = 0;
          comp_contrast = 0.0;
          zone_shadows = 1;
        } else if (measure_flag) {
          ev_shadows_total += ev;
          ev_shadows_count ++;
          ev_shadows = ev_shadows_total / ev_shadows_count;
        }

        if (method == METHOD_ZONE) 
          sprintf(buffer, "S%5s%5s %-4s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_shadows, 5, 2, buffer3), display_zone(zone_shadows, buffer4));
        else if (method == METHOD_COMP) 
          sprintf(buffer, "S%5s%5s %4s", dtostrf(ev, 5, 2, buffer2), dtostrf(ev_shadows, 5, 2, buffer3), dtostrf(comp_contrast, 4, 1, buffer4));
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
      lux_calibration = lux;
      ev_calibration = ev;
    }

    float d = log(lux_calibration / lux) / log(10);
    sprintf(buffer, "D%6s %5s", dtostrf(d, 6, 3, buffer2), dtostrf(ev_calibration - ev, 5, 2, buffer3));
    lcd.setCursor(0, 0);
    lcd.print(buffer);
  }
}

void reset() 
{
  lux = 0;
  ev = 0.0;
  ev_highlights = 0.0;
  ev_highlights_total = 0.0;
  ev_highlights_count = 0;
  ev_shadows = 0.0;
  ev_shadows_total = 0.0;
  ev_shadows_count = 0;
  comp_brightness = 0.0;
  comp_contrast = 0.0;
  zone_highlights = 8;
  zone_shadows = 1;
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
  if (now - last_interrupt < 100)
    return;

  // PIND & 0b00000100 isolates Pin 2 (Bit 2)
  // PIND & 0b00001000 isolates Pin 3 (Bit 3)
  bool clkState = PIND & _BV(PD2); 
  bool dtState  = PIND & _BV(PD3);

  if (method == METHOD_ZONE) {
    if (submode == SUBMODE_HIGHLIGHTS) {
      if (clkState != dtState && zone_highlights < 9) {
        zone_highlights ++;
      } else if (clkState == dtState && zone_highlights > 5) {
        zone_highlights --;
      }
    } else if (submode == SUBMODE_SHADOWS) {
      if (clkState != dtState && zone_shadows < 5) {
        zone_shadows ++;
      } else if (clkState == dtState && zone_shadows > 1) {
        zone_shadows --;
      }
    }
  } else if (method == METHOD_COMP) {
    if (submode == SUBMODE_HIGHLIGHTS) {
      if (clkState != dtState && comp_brightness < 2.0f) {
        comp_brightness += 0.5f;
      } else if (clkState == dtState && comp_brightness > -2.0f) {
        comp_brightness -= 0.5f;
      }
    } else if (submode == SUBMODE_SHADOWS) {
      if (clkState != dtState && comp_contrast < 2.0f) {
        comp_contrast += 0.5f;
      } else if (clkState == dtState && comp_contrast > -2.0f) {
        comp_contrast -= 0.5f;
      }
    }
  }
  
  last_interrupt = now;
}

char* display_zone(uint8_t zone, char* buffer) {
  switch (zone) {
  case 1: strcpy(buffer, "I"); break;
  case 2: strcpy(buffer, "II"); break;
  case 3: strcpy(buffer, "III"); break;
  case 4: strcpy(buffer, "IV"); break;
  case 5: strcpy(buffer, "V"); break;
  case 6: strcpy(buffer, "VI"); break;
  case 7: strcpy(buffer, "VII"); break;
  case 8: strcpy(buffer, "VIII"); break;
  case 9: strcpy(buffer, "IX"); break;
  default: strcpy(buffer, "");
  }

  return buffer;
}

// To calculate filter, we use quadratic function:
// filter = a + b * ev + c * ev * ev;
// quadratic in Horner's form:
// filter = (c * ev + b) * ev + a;

// 0.152528045  -2.695569866  8.743406703
float calculate_filter_rc5(const float ev)
{
  float filter = (0.152528045f * ev + -2.695569866f) * ev + 8.743406703f;

  filter = roundf(filter * 2.0f) * 0.5f;

  if (filter < 0.0f) return 0.0f;
  if (filter > 5.0f) return 5.0f;

  return filter;
}

// 0.082128023  -2.105486282  7.656978866
float calculate_filter_fb(const float ev)
{
    float filter = (0.082128023f * ev + -2.105486282f) * ev + 7.656978866f;

    filter = roundf(filter * 2.0f) * 0.5f;

    if (filter < 0.0f) return 0.0f;
    if (filter > 5.0f) return 5.0f;

    return filter;
}

// To clalculate exposure, 
// 3.07 is the EV value used in calibration for getting these exp times- measuring without any filter

uint16_t calculate_exposure_rc5(const float filter, const float ev_highlights)
{
  static float exp_times[] = { 
    2.3101f, 2.3500f,   // 0
    2.3898f, 2.5491f,   // 1
    2.7084f, 2.7881f,   // 2
    2.8678f, 2.9474f,   // 3
    5.5762f, 5.8152f,   // 4
    6.0542f };          // 5
  return round(exp_times[int(filter * 2)] * pow(2, (3.07 - ev_highlights)));
}

uint16_t calculate_exposure_fb(const float filter, const float ev_highlights)
{
  static float exp_times[] = { 
    2.3898f, 2.3898f,   // 0
    2.3898f, 2.3898f,   // 1
    2.3898f, 2.5491f,   // 2
    2.7084f, 2.8678f,   // 3
    4.7796f, 4.9588f,   // 4
    5.1779f };          // 5
  return round(exp_times[int(filter * 2)] * pow(2, (3.07 - ev_highlights)));
}
