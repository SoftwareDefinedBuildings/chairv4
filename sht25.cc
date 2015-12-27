
#include "sht25.h"
#include "religion.h"
using namespace firestorm;

SHT25::SHT25()
{
  storm::i2c::lock.acquire([this]
  {
    auto bf = mkbuf({0xFE}); //Reload command
    auto rv = i2c::write(CHIPADDR, i2c::START | i2c::STOP, bf, 1, [this](auto s, auto)
    {
      storm::i2c::lock.release();
    });
    if (rv == nullptr)
    {
      religion::enter_next_life(0);
    }
  });
}

void SHT25::_acquire(uint8_t code, int wait_time, std::function<void(int8_t, uint16_t)> onDone)
{
  storm::i2c::lock.acquire([=]
  {
    auto bf = mkbuf(2);
    (*bf)[0] = code; //Read temp in no blocking mode
    auto rv = i2c::write(CHIPADDR, i2c::START | i2c::STOP, bf, 1, [=](auto s, auto buf)
    {
      if (s)
      {
        printf("No temperature sensor:%d\n", s);
        storm::i2c::lock.release();
        tq::add([=]
        {
          onDone(-1,0);
        });
        return;
      }
      //Now there is an approx 85ms delay for acquisition.
      //Don't rush it
      storm::Timer::once(wait_time * storm::Timer::MILLISECOND, [=](auto)
      {
        auto rv = i2c::read(CHIPADDR, i2c::START | i2c::STOP, bf, 2, [=](auto s, auto)
        {
          if (s) //Should not happen
          {
            storm::i2c::lock.release();
            onDone(-1, 0);
            return;
          }
          uint16_t rv = ((*bf)[0] << 8) | ((*bf)[1]);
          rv &= ~3;
          storm::i2c::lock.release();
          onDone(0, rv);
        });
        if (rv == nullptr)
        {
          religion::enter_next_life(0);
        }
      });
    });
    if (rv == nullptr)
    {
      religion::enter_next_life(0);
    }
  });
}

void SHT25::acquire_real_temperature(std::function<void(int8_t, double)> onDone)
{
  _acquire(0b11110011, 95, [=](int8_t status, uint16_t v)
  {
    if (status)
    {
      tq::add([=]
      {
        onDone(status, 0);
      });
    }
    else
    {
      double rv = -46.85 + (175.72*v)/65536;
      tq::add([=]
      {
        onDone(0, rv);
      });
    }
  });
}
void SHT25::acquire_raw_temperature(std::function<void(int8_t, uint16_t)> onDone)
{
  _acquire(0b11110011, 95, [=](int8_t status, uint16_t v)
  {
    tq::add([=]
    {
      onDone(status, v);
    });
  });
}
void SHT25::acquire_real_humidity(std::function<void(int8_t, double)> onDone)
{
  _acquire(0b11110101, 40, [=](int8_t status, uint16_t v)
  {
    if (status)
    {
      tq::add([=]
      {
        onDone(status, 0);
      });
    }
    else
    {
      double rv = -6 + (125.00*v)/65536;
      tq::add([=]
      {
        onDone(0, rv);
      });
    }
  });
}
void SHT25::acquire_raw_humidity(std::function<void(int8_t, uint16_t)> onDone)
{
  _acquire(0b11110101, 40, [=](int8_t status, uint16_t v)
  {
    tq::add([=]
    {
      onDone(status, v);
    });
  });
}
