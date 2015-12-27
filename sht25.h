
#ifndef __SHT25_H__
#define __SHT25_H__

#include "libstorm.h"
#include "libchair.h"

class SHT25
{
public:
  SHT25();
  void acquire_real_temperature(std::function<void(int8_t, double)> onDone);
  void acquire_raw_temperature(std::function<void(int8_t, uint16_t)> onDone);
  void acquire_real_humidity(std::function<void(int8_t, double)> onDone);
  void acquire_raw_humidity(std::function<void(int8_t, uint16_t)> onDone);
private:
  void _acquire(uint8_t code, int wait_time, std::function<void(int8_t, uint16_t)> onDone);
  static constexpr uint16_t CHIPADDR = i2c::external(0x80);
};

#endif
