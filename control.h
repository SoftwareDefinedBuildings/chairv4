#ifndef __CONTROL_H__
#define __CONTROL_H__

#include "libstorm.h"
#include "libchair.h"
#include "logfs.h"
#include "sht25.h"
#include "mcp3425.h"

#include <stdint.h>
namespace firestorm
{
  class Controls
  {
  public:
    Controls(firestorm::LogFS &l, SHT25 &tmp, MCP3425 &adc, firestorm::RTCC &rtc);
    void setSettings(int8_t nback_fan, int8_t nbottom_fan, int8_t nback_heat, int8_t nbottom_heat);


  private:
    //This is in tick

    firestorm::LogFS &log;
    SHT25 &sht;
    MCP3425 &adc;
    firestorm::RTCC &rtc;
    //LL is last logged
    uint8_t last_wdt;
    void tick();
    void logtick();
    void acquireTH();
    void tickHeaters();
    void writeBootRecord();
    void upsync();
    void syncfans();
    void saveSettings();
    void loadSettings();

    bool busy_ticking;
    bool actuation_override;
    uint8_t ll_bottom_heat;
    uint8_t bottom_heat;
    uint8_t ll_bottom_fan;
    uint8_t bottom_fan;
    uint8_t ll_back_heat;
    uint8_t back_heat;
    uint8_t ll_back_fan;
    uint8_t back_fan;
    uint8_t ll_occupancy;
    uint8_t occupancy;

    uint16_t humidity;
    uint16_t ll_humidity;
    uint16_t temperature;
    uint16_t ll_temperature;

    uint16_t reset_count;
    bool every_other_second;
    uint32_t last_log_settings;
    uint32_t last_log_TH;
    uint32_t religion_rec;
    uint16_t bad_settings;
    int8_t voltage_tokens;
    bool battery_ok;
    static constexpr storm::gpio::Pin pin_occ = storm::gpio::D7;
    static constexpr storm::gpio::Pin pin_hca = storm::gpio::D4;
    static constexpr storm::gpio::Pin pin_hcb = storm::gpio::D3;
    static constexpr storm::gpio::Pin pin_fa0 = storm::gpio::A0;
    static constexpr storm::gpio::Pin pin_fa1 = storm::gpio::A1;
    static constexpr storm::gpio::Pin pin_fa2 = storm::gpio::A2;
    static constexpr storm::gpio::Pin pin_fb0 = storm::gpio::A3;
    static constexpr storm::gpio::Pin pin_fb1 = storm::gpio::A4;
    static constexpr storm::gpio::Pin pin_fb2 = storm::gpio::A5;


    static firestorm::I2CRegister<storm::i2c::external(0x6e), 0> status_led;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 4> top_leds;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 13> top_brightness;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 14> top_knob;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 17> bot_leds;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 26> bot_brightness;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 27> bot_knob;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 34> settings;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 37> kick_wdt;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 38> firmware_version;
    static firestorm::I2CRegister<storm::i2c::external(0x6e), 39> occupancy_reg;
  };
}

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

#endif
