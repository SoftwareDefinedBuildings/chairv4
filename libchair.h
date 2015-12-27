
#ifndef __LIBCHAIR_H__
#define __LIBCHAIR_H__

#include <stdio.h>
#include "interface.h"
#include <functional>
#include "libstorm.h"
#include <cstring>
#include <time.h>

using namespace storm;

namespace firestorm
{
  template <uint16_t devaddress, uint8_t regaddr>
  class I2CRegister
  {
  public:
    I2CRegister()
    {
    }

    static void read_offset(uint8_t offset, buf_t target, uint16_t length, std::function<void(int,buf_t)> const& callback)
    {
      i2c::lock.acquire([=]
      {
        auto addrbuf = mkbuf({(uint8_t)(regaddr+offset)});
        auto srv = i2c::write(devaddress, i2c::START, move(addrbuf), 1,
          [length,callback = move(callback),target = move(target)](int status, buf_t buf)
        {
          if (status != i2c::OK)
          {
            i2c::lock.release();
            callback(status, move(buf));
            return;
          }
          auto srv = i2c::read(devaddress, i2c::RSTART | i2c::STOP, move(target), length,
            [callback = move(callback),target = move(target)](int status, buf_t buf)
          {
            i2c::lock.release();
            callback(status, move(buf));
            return;
          });
          if (srv == nullptr)
          {
            i2c::lock.release();
            callback(i2c::SYSCALL_ERR, nullptr);
          }
        });
        if (srv == nullptr)
        {
          i2c::lock.release();
          callback(i2c::SYSCALL_ERR, nullptr);
        }
      });
    }

    static void read(buf_t target, uint16_t length, std::function<void(int,buf_t)> const& callback)
    {
      read_offset(0, target, length, callback);
    }

    static void write_offset(uint8_t offset, buf_t msg, uint16_t length, std::function<void(int,buf_t)>  const& callback)
    {
      i2c::lock.acquire([=]
      {
        auto msgbuf = mkbuf(length+1);
        std::memcpy(&(*msgbuf)[1], &(*msg)[0], length);
        (*msgbuf)[0] = regaddr + offset;

        auto srv = i2c::write(devaddress, i2c::START | i2c::STOP, move(msgbuf), length+1,
          [callback,msg = move(msg)](int status, buf_t buf)
        {
          //We don't use the new buffer we made, rather return the buffer the user
          //gave us
          i2c::lock.release();
          callback(status, move(buf));
        });
        if (srv == nullptr)
        {
          i2c::lock.release();
          callback(i2c::SYSCALL_ERR, nullptr);
        }
      });
    }
    static void write(buf_t msg, uint16_t length, std::function<void(int,buf_t)>  const& callback)
    {
      write_offset(0, msg, length, callback);
    }
  };

  class TMP006
  {
  public:
    TMP006()
     : okay(false)
    {
      //Reset the chip and sample at 1/sec
      buf_t cfg = mkbuf({0b11110100});
      config.write(cfg, 1, [&](int status,auto){
        okay = (status == i2c::OK);
      });
    }
    void getDieTemp(std::function<void(double)> const& result)
    {
      buf_t rv = mkbuf(2);
      temp.read(move(rv), 2, [result](int status, buf_t buf){
        if (status != i2c::OK)
        {
          result(-1);
          return;
        }
        uint16_t temp = (((uint16_t)(*buf)[0] << 8) + (*buf)[1]) >> 2;
        double rtemp = (double)temp * 0.03125;
        result(rtemp);
      });
    }
  private:
    bool okay;
    static I2CRegister<i2c::TMP006, 2> config;
    static I2CRegister<i2c::TMP006, 2> temp;
    static I2CRegister<i2c::TMP006, 0> sensor;
  };

  struct rtcc_time_t
  {
    bool ok;
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
  };
  class RTCC
  {
  public:
    RTCC()
      {
        /*auto buf = mkbuf({0x80, 11,0, 0x08});
        maintime.write(buf, 4, [](int status, auto)\
        {
          printf("init stat %d\n", status);
        });*/
        //In initial config ensure
        //24 hour mode (register 0x02)
        //osc start (register 0x00)
        //vbaten register 0x03
        //pwrfail register 0x03
      }

      void setTime(rtcc_time_t &dt, std::function<void(int)> const& cb)
      {
        auto buf = mkbuf({
          uint8_t(0x80 | ((dt.sec/10)<<4)   | (dt.sec%10)), //enable osc
          uint8_t(       ((dt.min/10)<<4)   | (dt.min%10)),
          uint8_t(       ((dt.hour/10)<<4)  | (dt.hour%10)), //enable 24h
          uint8_t(0x08), //enable battery
          uint8_t(       ((dt.day/10)<<4)   | (dt.day%10)),
          uint8_t(       ((dt.month/10)<<4) | (dt.month%10)),
          uint8_t(       ((dt.year/10)<<4)  | (dt.year%10)),
          //uint8_t(0x80),
          uint8_t(0xc1) //enable sqrwaveout at 4096 Hz
        });
        maintime.write(move(buf), 8, [cb](int status, buf_t res)
        {
          cb(status);
        });
      }

      void writeSRAM(uint8_t addr, buf_t value, uint8_t len, std::function<void(int)> cb)
      {
        sram.write_offset(addr, move(value), len, [cb](int status, buf_t res)
        {
          cb(status);
        });
      }
      void readSRAM(uint8_t addr, buf_t dest, uint8_t len, std::function<void(int, buf_t)> cb)
      {
        sram.read_offset(addr, move(dest), len, cb);
      }
      void calibratePrompt()
      {
        printf("Enter date in form y/m/d H:M:S\n");
        int y,m,d,H,M,S;
        scanf("%d/%d/%d %d:%d:%d\n", &y, &m, &d, &H, &M, &S);
        printf("scanf done\n");
        rtcc_time_t dt;
        dt.year = y;
        dt.month = m;
        dt.day = d;
        dt.hour = H;
        dt.min = M;
        dt.sec = S;
        setTime(dt, [](int status)
        {
          printf("Done: %d\n", status);
        });
      }

      void binary_to_date(unsigned long binary, rtcc_time_t &dt)
      {
        time_t tt = binary;
        struct tm *rv = gmtime(&tt);
        dt.year = rv->tm_year - 100;
        dt.month = rv->tm_mon + 1;
        dt.day = rv->tm_mday;
        dt.hour = rv->tm_hour;
        dt.min = rv->tm_min;
        dt.sec = rv->tm_sec;
      }



      // this array represents the number of days in one non-leap year at
      //    the beginning of each month
      static constexpr uint32_t days_to_months[13] =
      {
          0,31,59,90,120,151,181,212,243,273,304,334,365
      };

      static uint32_t date_to_binary(rtcc_time_t &dt)
      {
         uint32_t iday;
         uint32_t val;
         iday = 365 * (dt.year + 30) + days_to_months[dt.month-1] + (dt.day - 1);
         iday = iday + (dt.year + 31) / 4;
         if ((dt.month > 2) && ((dt.year % 4) == 0))
         {
            iday++;
         }
         val = dt.sec + 60 * dt.min + 3600 * (dt.hour + 24 * iday);
         return val;
      }

      void getUnixTime(std::function<void(uint32_t)> const& result)
      {
        getRTCTime([result](rtcc_time_t t)
        {
          result(date_to_binary(t));
        });
      }
      void getRTCTime(std::function<void(rtcc_time_t)> const& result)
      {
        buf_t res = mkbuf(7);
        maintime.read(move(res), 7, [result](int status, buf_t res)
        {
          rtcc_time_t rv;
          if (status != i2c::OK)
          {
            printf("got status %d\n", status);
            rv.ok = false;
            result(rv);
            return;
          }
          rv.sec = (*res)[0] & 0b1111;
          rv.sec += (((*res)[0] >> 4) & 0b111)*10;
          rv.min = (*res)[1] & 0b1111;
          rv.min += (((*res)[1] >> 4) & 0b111)*10;
          rv.hour = (*res)[2] & 0b1111;
          rv.hour += (((*res)[2] >> 4) & 0b11)*10;
          rv.day = (*res)[4] & 0b1111;
          rv.day += (((*res)[4] >> 4) & 0b11)*10;
          rv.month = (*res)[5] & 0b1111;
          rv.month += (((*res)[5] >> 4) & 0b1)*10;
          rv.year = ((*res)[6] & 0b1111);
          rv.year += (((*res)[6] >> 4) & 0b1111)*10;
          rv.ok = true;
          result(rv);
        });
      }
      void getRawRegisters(std::function<void(buf_t)> const& result)
      {
        buf_t res = mkbuf(9);
        maintime.read(move(res), 9, [result](int status, buf_t res)
        {
          if (status != i2c::OK)
          {
            printf("got status %d\n", status);
            return;
          }
          result(std::move(res));
        });
      }
      void getStringTime(std::function<void(std::string)> const& result)
      {
        getRTCTime([result](rtcc_time_t rv)
        {
          char buf[80];
          if (!rv.ok)
          {
            result(std::string("BADTIME"));
          }
          snprintf(buf, sizeof(buf), "20%02d/%02d/%02d %02d:%02d:%02d",
            rv.year, rv.month, rv.day, rv.hour, rv.min, rv.sec);
          result(std::string(buf));
        });
      }

  private:
    static I2CRegister<i2c::external(0xDE), 0x00> maintime;
    static I2CRegister<i2c::external(0xDE), 0x07> control;
    static I2CRegister<i2c::external(0xDE), 0x18> pd_time;
    static I2CRegister<i2c::external(0xDE), 0x1c> pu_time;
    static I2CRegister<i2c::external(0xDE), 0x20> sram;
  };
}

#endif
