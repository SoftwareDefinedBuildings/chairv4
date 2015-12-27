
#include "religion.h"
#include "logfs.h"
#include "libchair.h"
#include "libstorm.h"
#include <inttypes.h>

//
// Types of records:
//
// === unallocated
// 1111 ????
//
// === timestamp
// 1110 tttt
//  24t      : 28 bit full timestamp
//
// === temp/humidity/occupancy
// 00tt tOhh
//   8h : 12 bit humidity
// hhmm mmmm
//   8m : 14 bit temperature
//
// === settings t=time, h = bottom heat, H = back heat, c = bottom cool, C = back cool
// 01tt hhhh
//  hhhH HHHH
//  HHcc cccc
//  cCCC CCCC
//
//
// [0] 28 bit full timestamp
//  h: timestamp
//  3: timestamp
//
// 00xx : temp/hum telemetry
// 01xx : settings
// 10xx
// 1100
// 1101
// 1110 : timestamp
// 1111 : unalloc

namespace firestorm
{


  LogFS::LogFS(firestorm::RTCC &rtc, std::function<void(void)> ondone)
    :rtc(rtc), oninitdone(ondone)
  {
    temp_record = mkbuf(RSIZE);
    blank_record = mkbuf({0xFF, 0xFF, 0xFF, 0xFF});
    printf("USERLAND VERSION: %d\n", FIRMWARE_VERSION);
    doSync([this,ondone]()
    {
      tq::add([=]
      {
        timeSynced = true;
        loadPointers([this,ondone]()
        {
          printf("LOADED LOG POINTERS read=%" PRId32 " write=%" PRId32 "\n", read_ptr, write_ptr);
          initialized = true;
          logTimestamp();
          tq::add(ondone);
          storm::Timer::periodic(3*storm::Timer::SECOND, [this](auto)
          {
              this->savePointers([]{});
          });
          storm::Timer::periodic(30*storm::Timer::SECOND, [this](auto)
          {
            this->doSync([=]
            {
              this->logTimestamp();
            });
          });
        });
      });
    });
  }

  //States
  //1 last block was free, looking for read ptr + write ptr
  //2 last block was occupied, looking for read ptr + write ptr
  //3 last block was free, looking for read ptr
  //4 last block was occupied, looking for read ptr
  //5 last block was free, looking for write ptr
  //6 last block was occupied, looking for write ptr
  void LogFS::_loadPointers(int state, int i, int endi, std::function<void()> ondone)
  {
    storm::sys::kick_wdt();
    if (i == endi) {
      tq::add(ondone);
      return;
    }
    if (i >= BMAX) {
      i -= BMAX;
    }
    storm::flash::read(i*RSIZE, temp_record, RSIZE, [=](int s, buf_t b)
    {
      if ((i&0xFF) == 0)
      {
        printf("Just did i=%d state=%d\n", i, state);
      }
      bool occ = (*b)[0] != 0xFF;
      int _state = state;
      switch (state)
      {
        case 1: //last free, need rptr + wptr
          if (occ)
          {
            read_ptr = i;
            _state = 6;
          }
          else
          {
            _state = 1;
          }
          break;
        case 2: //last occupied, need rptr + wptr
          if (occ)
          {
            _state = 2;
          }
          else
          {
            write_ptr = i;
            _state = 3;
          }
          break;
        case 3: //last free, need rptr
          if (occ)
          {
            read_ptr = i;
            tq::add(ondone);
            return;
          }
          else
          {
            _state = 3;
          }
          break;
        case 4: //last occupied, looking for read ptr
          if (occ)
          {
            _state = 4;
          }
          else
          {
            _state = 3;
          }
          break;
        case 5: //last free, looking for write ptr
          if (occ)
          {
            _state = 6;
          }
          else
          {
            _state = 5;
          }
          break;
        case 6: //last occupied, looking for write ptr
          if (occ)
          {
            _state = 6;
          }
          else
          {
            write_ptr = i;
            tq::add(ondone);
            return;
          }
          break;
        default:
          religion::enter_next_life(state);
      }
      tq::add([=](){_loadPointers(_state, i+1, endi, ondone);});
    });
  }

  void LogFS::readRecord(std::function<void(buf_t)> onDone)
  {
    if (read_ptr == write_ptr ||
       ((read_ptr + 1) % BMAX) == write_ptr)
    {
      //Log is empty, or only has one record
      tq::add([onDone]
      {
        onDone(nullptr);
      });
      return;
    }
    else
    {
      auto rv = mkbuf(RSIZE);
      storm::flash::read(read_ptr*RSIZE, rv, RSIZE, [=](int s, buf_t b)
      {
        storm::flash::write(read_ptr*RSIZE, blank_record, RSIZE, [onDone, b](int s, buf_t b2)
        {
          if (s) religion::enter_next_life(s);
          tq::add([=]
          {
            onDone(b);
          });
        });
        read_ptr ++;
        if (read_ptr == BMAX)
          read_ptr -= BMAX;
      });
    }
  }
  void LogFS::savePointers(std::function<void()> ondone)
  {
    auto buf = mkbuf(8);
    (*buf)[0] = write_ptr & 0xFF;
    (*buf)[1] = (write_ptr >> 8) & 0xFF;
    (*buf)[2] = (write_ptr >> 16) & 0xFF;
    (*buf)[3] = 0x5a;
    (*buf)[4] = read_ptr & 0xFF;
    (*buf)[5] = (read_ptr >> 8) & 0xFF;
    (*buf)[6] = (read_ptr >> 16) & 0xFF;
    (*buf)[7] = 0x5a;
    rtc.writeSRAM(0, buf, 8, [=](int status){
      if (status) religion::enter_next_life(status);
      tq::add(ondone);
    });
  }
  void LogFS::bootstrapPointersWithHint(int write_hint, int read_hint, std::function<void()> ondone)
  {
    if (write_hint == 0 && read_hint == 0) {
      //If there are no pointers in SRAM, we can't afford to scan the log, as it takes too long.
      //So we don't invoke ondone(), and initialized will be left false. That way all public
      //logging methods will fail, but factoryReset() can still be called. The periodic
      //logTimestamps won't be set up, so the board can be reset from the cmd line
      printf("FACTORY RESET STATE, ABORTING DIRTY");
      return;
    }
    //Rollback some to add ruggedness
    write_hint -= 10;
    read_hint -= 10;
    if (write_hint < 0) write_hint += BMAX;
    if (read_hint < 0) read_hint += BMAX;
    int pre_write = write_hint - 1;
    int pre_read = read_hint - 1;
    if (pre_write < 0) pre_write += BMAX;
    if (pre_read < 0) pre_read += BMAX;
    printf("pre addrs w/r %d/%d\n", pre_write, pre_read);
    write_ptr = -1;
    read_ptr = -1;
    storm::flash::lock.acquire([=]
    {
      storm::flash::read(pre_write*RSIZE, temp_record, RSIZE, [=](int s, buf_t b)
      {
        bool occ = (*b)[0] != 0xFF;
        printf("prewrite occ: %d\n", occ?1:0);
        int state = occ ? 6 : 5;
        tq::add([=]()
        {
          _loadPointers(state, write_hint, pre_write, [=]()
          {
            if (write_ptr == -1)
            {
              printf("No write pointer found, using default reset\n");
              write_ptr = 0;
              read_ptr = 0;
              logTimestamp();
              storm::flash::lock.release();
            }
            else
            {
              printf("Found write pointer\n");
              storm::flash::read(pre_read*RSIZE, temp_record, RSIZE, [=](int s, buf_t b)
              {
                bool occ = (*b)[0] != 0xFF;
                int state = occ ? 4 : 3;
                printf("preread occ: %d\n", occ?1:0);
                tq::add([=]()
                {
                  _loadPointers(state, read_hint, pre_read, [=]()
                  {
                    printf("Found read pointer\n");
                    if (read_ptr == -1)
                    {
                      printf("No read ptr found (but found write?)");
                      religion::enter_next_life(0);
                    }
                    //Mast out the bottom bits so we are aligned with batches
                    read_ptr &= ~0xf;
                    storm::flash::lock.release();
                    ondone();
                  });
                });
              });
            }
          });
        });
      });
    });
  }
  void LogFS::bootstrapPointersWithNoHint(std::function<void()> ondone)
  {
    bootstrapPointersWithHint(0, 0, ondone);
  }
  void LogFS::peekBatch(std::function<void(buf_t, uint32_t)> onDone)
  {
    //Assume that RMAX is a multiple of BATCHSIZE so our reads will
    //never cross the end of flash
    int32_t delta = write_ptr - read_ptr;
    if (read_ptr % BATCHSIZE != 0)
      religion::enter_next_life(read_ptr);
    if (delta < 0) delta += BMAX;
    if (delta <= BATCHSIZE+1)
    {
      //Don't do any peeks if we have less than BATCHSIZE records
      onDone(nullptr, read_ptr);
      return;
    }

    printf("OK on batch: r=%" PRId32 " w=%" PRId32 "\n", read_ptr, write_ptr);
    uint32_t frozen_read_ptr = read_ptr;
    auto rv = mkbuf(RSIZE*BATCHSIZE);
    storm::flash::lock.acquire([=]
    {
      storm::flash::read(frozen_read_ptr*RSIZE, rv, RSIZE*BATCHSIZE, [=](int s, buf_t b)
      {
        storm::flash::lock.release();
        if (s)
          religion::enter_next_life(0);
        onDone(b, frozen_read_ptr);
      });
    });
  }
  void LogFS::releaseBatch(uint32_t addr, uint32_t ts, std::function<void()> onDone)
  {
    //Initial fast-check
    if ((int32_t)addr != read_ptr) {
      printf("Release of wrong range, got %" PRIu32 " != %" PRId32 "\n", addr, read_ptr);
      tq::add(onDone);
      return;
    }
    int32_t ts_delta = (ts - EPOCH) - getAbsTimestamp();
    if (ts_delta > 60 || ts_delta < -60)
    {
      printf("We got a TS delta of %" PRId32 " seconds\n", ts_delta);
      //Our timebase is more than 2 minutes away from the server. Lets adjust
      //TODO evaluate consequences of interlacing/racing
      rtcc_time_t t;
      rtc.binary_to_date(ts, t);
      printf("ADJUSTING TS TO : %d/%d/%d %d:%d:%d\n", t.year, t.month, t.day, t.hour, t.min, t.sec);
      rtc.setTime(t, [=](auto){
        tq::add([=]{this->doSync([]{});});
      });
    }

    /*
    if (((read_ptr + BATCHSIZE) % BATCHSIZE) >= write_ptr) {
      printf("Got some F'd up release\n");
      tq::add(onDone);
      return;
    }*/
    auto bf = mkbuf(RSIZE*BATCHSIZE);
    for (int i = 0; i < BATCHSIZE*RSIZE; i++)
      (*bf)[i] = 0xFF;
    storm::flash::lock.acquire([=]
    {
      //There is a race above, so now we need to check again
      if ((int32_t)addr != read_ptr) {
        printf("Release2 of wrong range, got %" PRIu32 " != %" PRId32 "\n", addr, read_ptr);
        tq::add(onDone);
        return;
      }

      storm::flash::write(read_ptr*RSIZE, bf, RSIZE*BATCHSIZE, [=](int s, buf_t b2)
      {
        if (s) religion::enter_next_life(s);
        read_ptr += BATCHSIZE;
        if (read_ptr >= BMAX)
          read_ptr -= BMAX;
        printf("Successful release\n");
        storm::flash::lock.release();
        savePointers(onDone);
      });
    });
  }
  void LogFS::loadPointers(std::function<void()> ondone)
  {
    printf("Loading pointers\n");
    auto buf = mkbuf(8);
    rtc.readSRAM(0, buf, 8, [=](int status, buf_t buf)
    {
      if (status)
      {
        religion::enter_next_life(status);
      }
      if ((*buf)[3] == 0x5a && (*buf)[7] == 0x5a)
      {
        int write_hint, read_hint;
        write_hint = (*buf)[0];
        write_hint |= (*buf)[1] << 8;
        write_hint |= (*buf)[2] << 16;
        read_hint = (*buf)[4];
        read_hint |= (*buf)[5] << 8;
        read_hint |= (*buf)[6] << 16;
        printf("Located SRAM hint w/r %d %d\n", write_hint, read_hint);
        bootstrapPointersWithHint(write_hint, read_hint, ondone);
      }
      else
      {
        printf("Could not locate SRAM hint\n");
        bootstrapPointersWithNoHint(ondone);
      }
    });

  }

  void LogFS::doSync(std::function<void(void)> onDone)
  {
    rtc.getUnixTime([this, onDone](uint32_t t)
    {
      uint32_t n = storm::sys::now(storm::sys::SHIFT_16);
      lastRTCCTime = t - EPOCH;
      lastTicks = n;
      onDone();
    });
  }
  void LogFS::schedule_queue()
  {
    if (!flashOpBusy && !insert_queue.empty())
    {
      if (((write_ptr + 1) % BMAX) != read_ptr)
      {
        flashOpBusy = true;
        printf("ADDING LOG TO FLASH %" PRId32 "\n", write_ptr);
        //Log is not full
        storm::flash::lock.acquire([=]
        {
          tq::add([this]
          {
            buf_t b = insert_queue.front();
            insert_queue.pop();
            storm::flash::write(write_ptr*RSIZE, b, RSIZE, [this](int status, auto b)
            {
              if (status)
              {
                religion::enter_next_life(0);
              }
              write_ptr++;
              if (write_ptr >= BMAX) write_ptr -= BMAX;
              flashOpBusy = false;
              storm::flash::lock.release();
              storm::sys::kick_wdt();
              this->schedule_queue();
            });
          });
        });
      }
      else
      {
        printf("FLASH LOG IS FULL! DROPPING RECORD!\n");
        insert_queue.pop();
      }
    }
    // else
    // {
    //   printf("no schedule %d %d\n", flashOpBusy, insert_queue.empty());
    // }
  }
  void LogFS::insertRecord(buf_t b)
  {
    insert_queue.push(b);
    schedule_queue();
  }
  uint32_t LogFS::getRelTimestamp()
  {
    return getAbsTimestamp() - lastLogTime;
  }
  bool LogFS::isReady()
  {
    return initialized;
  }
  uint32_t LogFS::getAbsTimestamp()
  {
    //We tick at 5.7220458984375 ticks per second.
    //or 0.17476266666666668 seconds per tick
    uint32_t t = storm::sys::now(storm::sys::SHIFT_16);
    if (t < lastTicks) religion::enter_next_life(t);
    t -= lastTicks;
    t *= 1747;
    t /= 10000;
    //t is now the number of seconds since the last RTC timestamp
    return lastRTCCTime + t;
  }
  void LogFS::incLastTimestamp(uint32_t t)
  {
    lastLogTime += t;
  }
  void LogFS::setLastTimestamp(uint32_t t)
  {
    lastLogTime = t;
  }
  void LogFS::logTimestamp()
  {
    uint32_t t = getAbsTimestamp();
    auto r = mkbuf({
        (uint8_t)(T_TIMESTAMP | ((t >> 24) & 0xF)),
        (uint8_t)((t>>16) & 0xFF),
        (uint8_t)((t>>8) & 0xFF),
        (uint8_t)(t & 0xFF)
      });
    setLastTimestamp(t);
    insertRecord(r);
  }
  void LogFS::logBootRecord(uint16_t reset_count, uint32_t religion_rec)
  {
    if(!initialized) return;
    logTimestamp();

    if (religion_rec != 0) religion_rec -= 0x50000;
    auto r = mkbuf({
      (uint8_t)(T_BOOTREC | FIRMWARE_VERSION),
      (uint8_t)((reset_count << 2) | religion_rec >> 16),
      (uint8_t)(religion_rec >> 8),
      (uint8_t)(religion_rec)
    });
    insertRecord(r);
  }
  void LogFS::logTempHumidityOccupancy(uint16_t temp, uint16_t humidity, bool occupancy)
  {
    if(!initialized) return;
    //Values are left justified, make them right justified.
    temp >>= 2;
    humidity >>= 4;
    uint32_t t = getRelTimestamp();
    if (t > 7) {
      logTimestamp();
      t = 0;
    }
    auto r = mkbuf({
      (uint8_t)(T_TEMPHUM | ((t << 3) & 0b111000) | (occupancy << 2) | ((humidity >> 10) & 0b11)),
      (uint8_t)((humidity >> 2) & 0xFF),
      (uint8_t)(((humidity << 6) & 0b11000000) | ((temp >> 8) & 0b00111111)),
      (uint8_t)(temp)
    });
    incLastTimestamp(t);
    insertRecord(r);
  }
  void LogFS::logSettings(uint8_t backCool, uint8_t backHeat, uint8_t bottomCool, uint8_t bottomHeat)
  {
    if(!initialized) return;
    uint32_t t = getRelTimestamp();
    if (t > 3) {
      logTimestamp();
      t = 0;
    }
    auto r = mkbuf({
      (uint8_t)(T_SETTING | (t << 4) | (bottomHeat >> 3)),
      (uint8_t)((bottomHeat << 5) | ((backHeat >> 2) & 0b00011111)),
      (uint8_t)((backHeat << 6) | ((bottomCool >> 1) & 0b00111111)),
      (uint8_t)((bottomCool << 7) | (backCool & 0x7f))
    });
    incLastTimestamp(t);
    insertRecord(r);
  }
  void LogFS::logBatteryVoltageK(uint16_t v, bool battery_ok)
  {
    if(!initialized) return;
    uint32_t t = getRelTimestamp();
    if (t > 2048) {
      logTimestamp();
      t = 0;
    }
    auto r = mkbuf({
      (uint8_t)(T_BATTERY | (battery_ok << 3) | (t >> 8)),
      (uint8_t)(t),
      (uint8_t)(v >> 8),
      (uint8_t)(v)
    });
    incLastTimestamp(t);
    insertRecord(r);
  }
  void LogFS::factoryReset(std::function<void()> ondone)
  {
    read_ptr = 0;
    write_ptr = 0;
    savePointers([=]()
    {
      auto cnt = std::make_shared<int>(200); //Average of seconds max to erase chip
      storm::flash::erase_chip();
      std::shared_ptr<storm::Timer> tmr = storm::Timer::periodic(1*storm::Timer::SECOND, [=](auto t){
        printf("CHIP ERASE: %d seconds left\n", *cnt);
        storm::sys::kick_wdt();
        *cnt -= 1;
        if (*cnt == 0) {
          t->cancel();
          tq::add([this,ondone]{
            this->logTimestamp();
            storm::Timer::once(2*storm::Timer::SECOND, [=](auto)
            {
              this->savePointers(ondone);
            });
          });

        }
      });
    });
  }

}
