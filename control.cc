
#include "control.h"

/**
 * REGISTER MAP
 * 0:  STATUS R
 * 1:  STATUS G
 * 2:  STATUS B
 * 3:  STATUS MODE
 * 4:  TOP LED 0
 * 5:  TOP LED 1
 * 6:  TOP LED 2
 * 7:  TOP LED 3
 * 8:  TOP LED 4
 * 9:  TOP LED 5
 * 10: TOP LED 6
 * 11: TOP LED 7
 * 12: TOP LED 8
 * 13: TOP BRIGHTNESS
 * 14: TOP KNOB R
 * 15: TOP KNOB G
 * 16: TOP KNOB B
 * 17: BOT LED 0
 * 18: BOT LED 1
 * 19: BOT LED 2
 * 20: BOT LED 3
 * 21: BOT LED 4
 * 22: BOT LED 5
 * 23: BOT LED 6
 * 24: BOT LED 7
 * 25: BOT LED 8
 * 26: BOT BRIGHTNESS
 * 27: BOT KNOB R
 * 28: BOT KNOB G
 * 29: BOT KNOB B
 * 30: TOP CLICKS
 * 31: BOT CLICKS
 * 32: TOP PRESSES
 * 33: BOT PRESSES
 * 34: TOP SETTING
 * 35: BOT SETTING
 * 36: ENABLE AUTOSET
 * 37: KICK WDT + STORE
 * 38: FIRMWARE VERSION
 */

#define TICK_INTERVAL
firestorm::Controls::Controls(firestorm::LogFS &l, SHT25 &tmp, MCP3425 &adc, firestorm::RTCC &rtc)
 : log(l), sht(tmp), adc(adc), rtc(rtc),
   busy_ticking(false), actuation_override(true), reset_count(0),

   every_other_second(false), religion_rec(0),  bad_settings(0), voltage_tokens(0),  battery_ok(true)
{
  //Occupancy sensor is on IOL8 = P7 = GP6
  //HCAD is on IOL5 = P4 = GP13
  //HCBD is on IOL4 = P3 = GP12
  //FCA0 is on AD0 = AN5
  //FCA1 is on AD1 = AN4
  //FCA2 is on AD2 = AN3
  //FCB0 is on AD3 = AN2
  //FCB1 is on AD4 = AN1
  //FCB2 is on AD5 = AN0

   storm::gpio::set_mode(pin_occ, storm::gpio::IN);
   storm::gpio::set_pull(pin_occ, storm::gpio::UP);
   storm::gpio::set_mode(pin_hca, storm::gpio::OUT);
   storm::gpio::set_mode(pin_hcb, storm::gpio::OUT);
   storm::gpio::set_mode(pin_fa0, storm::gpio::OUT);
   storm::gpio::set_mode(pin_fa1, storm::gpio::OUT);
   storm::gpio::set_mode(pin_fa2, storm::gpio::OUT);
   storm::gpio::set_mode(pin_fb0, storm::gpio::OUT);
   storm::gpio::set_mode(pin_fb1, storm::gpio::OUT);
   storm::gpio::set_mode(pin_fb2, storm::gpio::OUT);

   auto vbuf = mkbuf(1);
   firmware_version.read(vbuf,1,[=](auto status, auto val)
   {
     if (status != 0)
     {
       printf("COULD NOT CONNECT TO UI BOARD\n");
       religion::enter_next_life(status);
     }
     if ((*val)[0] != 0x13)
     {
       printf("BAD UI BOARD VERSION\n");
       religion::enter_next_life(0xBADBAD);
     }
   });
   auto bf = mkbuf(4);
   (*bf)[0] = 0;
   (*bf)[1] = 1;
   (*bf)[2] = 0;
   (*bf)[3] = 1;
   status_led.write(bf,4, [=](auto, auto){});

   storm::Timer::periodic(200*storm::Timer::MILLISECOND, [this](auto)
   {
     this->tick();
   });
   storm::Timer::periodic(10*storm::Timer::MILLISECOND, [this](auto)
   {
     this->tickHeaters();
   });
   //This was 2 seconds
   #warning still have demo hack
   storm::Timer::periodic(500*storm::Timer::MILLISECOND, [this](auto)
   {
     this->logtick();
   });
   storm::Timer::periodic(1*storm::Timer::SECOND, [this](auto)
   {
     auto b = mkbuf(1);
     kick_wdt.read(b, 1, [=](auto st, auto bb)
     {
       if (st != 0)
       {
         printf("Could not kick UI WDT\n");
         return;
       }
       last_wdt = (*b)[0];
     });
     this->acquireTH();
     this->saveSettings();
   });
   this->loadSettings();
   /*
   auto b = mkbuf(2);
   (*b)[0] = -100;
   (*b)[1] = 0;
   printf("O Settings %d %d\n", (int)(int8_t)((*b)[0]), (int)(int8_t)((*b)[1]));
   storm::Timer::periodic(1*storm::Timer::SECOND, [this,b](auto)
   {
      (*b)[0] += 25;
     if ( ((int8_t)(*b)[0]) > 100)
        (*b)[0] = -100;
      (*b)[1] -= 25;
     if (((int8_t)(*b)[1]) < -100)
        (*b)[1] = 100;
      printf("Settings %d %d\n", (int)(int8_t)((*b)[0]), (int)(int8_t)((*b)[1]));
     settings.write(b, 2, [](auto st, auto)
     {
       printf("Write status: %d\n", st);
     });
   });
   */

}

void firestorm::Controls::saveSettings()
{
  auto buf = mkbuf(8);
  (*buf)[0] = back_fan;
  (*buf)[1] = back_heat;
  (*buf)[2] = bottom_fan;
  (*buf)[3] = bottom_heat;
  (*buf)[4] = reset_count >> 8;
  (*buf)[5] = reset_count & 0xFF;
  (*buf)[6] = 0xa5;
  (*buf)[7] = 0x5a;
  //SRAM 0-7 is flash log
  //SRAM 8-15 is settings
  //SRAM 16-19 is religion
  rtc.writeSRAM(8, buf, 8, [=](int status){
    if (status) religion::enter_next_life(status);
  });
}
void firestorm::Controls::writeBootRecord()
{
  if (!log.isReady())
  {
    //printf("LOG NOT READY\n");
    storm::Timer::once(50*storm::Timer::MILLISECOND, [=](auto)
    {
      this->writeBootRecord();
    });
    return;
  }
  log.logBootRecord(reset_count, religion_rec);
}
void firestorm::Controls::upsync()
{
  //The UI knob is somewhat limited, so we will prefer cooling over heating
  actuation_override = true;
  int8_t back_setting;
  int8_t bottom_setting;
  if (back_heat != 0)
    back_setting = -back_heat;
  else
    back_setting = back_fan;

  if (bottom_heat != 0)
    bottom_setting = -bottom_heat;
  else
    bottom_setting = bottom_fan;
  auto sbuf = mkbuf({
    (uint8_t)bottom_setting,
    (uint8_t)back_setting
  });
  settings.write(sbuf, 2, [=](auto, auto){

  });

}
void firestorm::Controls::loadSettings()
{
  actuation_override = true;
  auto buf = mkbuf(12);
  rtc.readSRAM(8, buf, 12, [=](auto status, auto buf){
    if (status) religion::enter_next_life(status);
    if ((*buf)[6] == 0xa5 && (*buf)[7] == 0x5a)
    {
      back_fan = (*buf)[0];
      back_heat = (*buf)[1];
      bottom_fan = (*buf)[2];
      bottom_heat = (*buf)[3];
      reset_count = ((uint16_t)(*buf)[4] << 8) + (*buf)[5];
      reset_count += 1;
      religion_rec = ((uint32_t)(*buf)[8] << 24) + ((uint32_t)(*buf)[9] << 16) + ((uint32_t)(*buf)[10] << 8) + (uint32_t)(*buf)[11];

      tq::add([this]
      {
        this->writeBootRecord();
        religion::set_record(0);
      });
      tq::add([this]
      {
        this->upsync();
      });

    }
  });
}
void firestorm::Controls::setSettings(int8_t nback_fan, int8_t nbottom_fan, int8_t nback_heat, int8_t nbottom_heat)
{
  actuation_override = true;
  busy_ticking = true;
  if (nback_fan >= 0)
    back_fan = nback_fan;
  if (nbottom_fan >= 0)
    bottom_fan = nbottom_fan;
  if (nback_heat >= 0)
    back_heat = nback_heat;
  if (nbottom_heat >= 0)
    bottom_heat = nbottom_heat;
  tq::add([this]
  {
    this->upsync();
    actuation_override = true;
    busy_ticking = false;
  });
}
void firestorm::Controls::tick()
{
  if (busy_ticking)
    return;
  busy_ticking = true;

  //Read the latest settings from external controller
  //If we have received a network override, delay setting
  //sync for one tick to prevent races
  if (actuation_override)
  {
    printf("Hit actuation override\n");
    actuation_override = false;
    busy_ticking = false;
  }
  else
  {
    auto b = mkbuf(2);
    settings.read(b, 2, [=](auto st, auto bb)
    {
      if (st) {
        printf("BAD SETTINGS READ!\n");
        bad_settings += 1;
        if (bad_settings > 10)
          religion::enter_next_life(bad_settings);
        busy_ticking = false;
        return;
      }
      //Reversed for CBE
      int8_t top = (*b)[1];
      int8_t bottom = (*b)[0];
      //printf("Read settings %d %d\n", top, bottom);
      if (top > 0)
      {
        back_fan = top;
        back_heat = 0;
      }
      else
      {
        back_heat = -top;
        back_fan = 0;
      }
      if (bottom > 0)
      {
        bottom_fan = bottom;
        bottom_heat = 0;
      }
      else
      {
        bottom_heat = -bottom;
        bottom_fan = 0;
      }
      busy_ticking = false;
    });
    auto bf = mkbuf(6);
    if (battery_ok)
    {
      (*bf)[0] = 0;
      (*bf)[1] = 1;
      (*bf)[2] = 0;
      (*bf)[3] = 0;
    } else {
      (*bf)[0] = 1;
      (*bf)[1] = 0;
      (*bf)[2] = 0;
      (*bf)[3] = 1;
    }

    status_led.write(bf,4, [=](auto, auto){
      (*bf)[0] = occupancy && battery_ok;
      occupancy_reg.write(bf,1,[=](auto, auto){
        (*bf)[0] = 0;
        (*bf)[1] = 0;
        (*bf)[2] = 0;
        top_knob.write(bf, 3, [=](auto, auto){
          bot_knob.write(bf, 3, [=](auto, auto){

          });
        });
      });
    });
    this->syncfans();
  }
  occupancy = 1 - storm::gpio::get(pin_occ);
  syncfans();
}

void firestorm::Controls::syncfans()
{
  //Synchronize fan controls
  if (occupancy == 0 || back_fan == 0 || battery_ok == false)
  {
    storm::gpio::set(pin_fa0, 0);
    storm::gpio::set(pin_fa1, 0);
    storm::gpio::set(pin_fa2, 0);
  }
  else if (back_fan <= 25)
  {
    storm::gpio::set(pin_fa0, 1);
    storm::gpio::set(pin_fa1, 0);
    storm::gpio::set(pin_fa2, 0);
  }
  else if (back_fan <= 50)
  {
    storm::gpio::set(pin_fa0, 0);
    storm::gpio::set(pin_fa1, 1);
    storm::gpio::set(pin_fa2, 0);
  }
  else if (back_fan <= 75)
  {
    storm::gpio::set(pin_fa0, 1);
    storm::gpio::set(pin_fa1, 1);
    storm::gpio::set(pin_fa2, 0);
  }
  else
  {
    storm::gpio::set(pin_fa0, 1);
    storm::gpio::set(pin_fa1, 1);
    storm::gpio::set(pin_fa2, 1);
  }
  if (occupancy == 0 || bottom_fan == 0 || battery_ok == false)
  {
    storm::gpio::set(pin_fb0, 0);
    storm::gpio::set(pin_fb1, 0);
    storm::gpio::set(pin_fb2, 0);
  }
  else if (bottom_fan <= 25)
  {
    storm::gpio::set(pin_fb0, 1);
    storm::gpio::set(pin_fb1, 0);
    storm::gpio::set(pin_fb2, 0);
  }
  else if (bottom_fan <= 50)
  {
    storm::gpio::set(pin_fb0, 0);
    storm::gpio::set(pin_fb1, 1);
    storm::gpio::set(pin_fb2, 0);
  }
  else if (bottom_fan <= 75)
  {
    storm::gpio::set(pin_fb0, 1);
    storm::gpio::set(pin_fb1, 1);
    storm::gpio::set(pin_fb2, 0);
  }
  else
  {
    storm::gpio::set(pin_fb0, 1);
    storm::gpio::set(pin_fb1, 1);
    storm::gpio::set(pin_fb2, 1);
  }
}
constexpr storm::gpio::Pin firestorm::Controls::pin_occ;
constexpr storm::gpio::Pin firestorm::Controls::pin_hca;
constexpr storm::gpio::Pin firestorm::Controls::pin_hcb;
constexpr storm::gpio::Pin firestorm::Controls::pin_fa0;
constexpr storm::gpio::Pin firestorm::Controls::pin_fa1;
constexpr storm::gpio::Pin firestorm::Controls::pin_fa2;
constexpr storm::gpio::Pin firestorm::Controls::pin_fb0;
constexpr storm::gpio::Pin firestorm::Controls::pin_fb1;
constexpr storm::gpio::Pin firestorm::Controls::pin_fb2;

void firestorm::Controls::logtick()
{
  uint32_t now = storm::sys::now(storm::sys::SHIFT_16);
/*  if (ll_back_fan != back_fan ||
      ll_back_heat != back_heat ||
      ll_bottom_fan != bottom_fan ||
      ll_bottom_heat != bottom_heat ||
      (now - last_log_settings > 25)) //5s
  {*/
    log.logSettings(back_fan, back_heat, bottom_fan, bottom_heat);
    ll_back_fan = back_fan;
    ll_back_heat = back_heat;
    ll_bottom_fan = bottom_fan;
    ll_bottom_heat = bottom_heat;
    last_log_settings = now;
  //}


/*  if (ll_occupancy != occupancy ||
        ll_humidity != humidity ||
        ll_temperature != temperature)
        || (now - last_log_TH > 25)) //5s
  {*/
    log.logTempHumidityOccupancy(temperature, humidity, occupancy);
    ll_occupancy = occupancy;
    ll_humidity = humidity;
    ll_temperature = temperature;
    last_log_TH = now;

  adc.sample([=](auto st, auto val)
  {
    if (st) {
      printf("Bad ADC acquisition\n");
      return;
    }
    double voltage = ((double)val / 32768. * 2.048) / (10000. / (10000. + 68000.));
    if (voltage < 12.00) voltage_tokens--;
    else if (voltage > 12.8) voltage_tokens++;
    if (voltage_tokens < -10) voltage_tokens = -10;
    if (voltage_tokens > 10) voltage_tokens = 10;
    if (voltage_tokens == 10) battery_ok = true;
    if (voltage_tokens == -10) battery_ok = false;
    log.logBatteryVoltageK(val, battery_ok);
  });
//  }
}
void firestorm::Controls::tickHeaters()
{
  uint32_t now = storm::sys::now();
  uint32_t cyc = (now % 375000) / 3750;
  //cyc is now a number from 0 to 99
//  printf("bh %d\n", occupancy && cyc < back_heat);
  if (battery_ok)
  {
    storm::gpio::set(pin_hca, (occupancy && (cyc < bottom_heat))?1:0);
    storm::gpio::set(pin_hcb, (occupancy && (cyc < back_heat))?1:0);
  }
  else
  {
    storm::gpio::set(pin_hca, 0);
    storm::gpio::set(pin_hcb, 0);
  }

}
void firestorm::Controls::acquireTH()
{
  if (every_other_second)
  {
    sht.acquire_raw_humidity([=](auto status, auto val)
    {
      if (status != 0)
        humidity = 0;
      else
        humidity = val;
    });
  }
  else
  {
    sht.acquire_raw_temperature([=](auto status, auto val)
    {
      if (status != 0)
        temperature = 0;
      else
        temperature = val;

    });
  }
  every_other_second = !every_other_second;
}
