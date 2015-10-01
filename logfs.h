
#include "religion.h"

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


class LogFS
{
public:

  //Constants

  //midnight 1st jan 2015
  static constexpr const int EPOCH = 1420070400;
  static constexpr const int T_TIMESTAMP = 0b11100000;
  static constexpr const int T_TEMPHUM   = 0b00000000;
  static constexpr const int T_SETTING   = 0b01000000;
  static constexpr const int AMAX = 0x780000;
  static constexpr const int BMAX = 0x1e0000;
  static constexpr const int BPAGE = 64;
  static constexpr const int RSIZE = 4;
  //Time keeping
  firestorm::RTCC &rtc;
  //This lastRTCCTime is since the EPOCH above
  uint32_t lastRTCCTime;
  uint32_t lastTicks;

  //Relative time generation
  uint32_t lastLogTime;

  //reject all logging requests until we finish init
  bool initialized = false;
  bool pointersLoaded = false;
  bool timeSynced = false;
  std::function<void(void)> oninitdone;

  uint32_t read_block;
  uint32_t write_block;
  buf_t temp_record;

  LogFS(firestorm::RTCC &rtc, std::function<void(void)> ondone)
    :rtc(rtc), oninitdone(ondone)
  {
    temp_record = mkbuf(RSIZE);
  /*  doSync([]()
    {
      timeSynced = true;
      if (pointersLoaded)
      {
        initialized = true;
        tq::post([](
          ondone();
        ));
      }
    });*/
    loadPointers([](int a, int b)
    {
      printf("a=%d b=%d\n", a, b);
    });
  /*  loadPointers();
    if (timeSynced)
    {
      initialized = true;
      tq::post([](
        ondone();
      ));
    }*/
  }

  void _loadPointers(int fe, int le, int i, std::function<void(int, int)> ondone)
  {
    if (i >= BMAX) {
      ondone(fe, le);
      return;
    }
    storm::flash::read(i*RSIZE, temp_record, RSIZE, [=](int s, buf_t b)
    {
      int _fe = fe;
      int _le = le;
      if ((i&0xFF) == 0)
      {
        printf("Just did i=%d\n", i);
      }
      if ( (*b)[0] == 0xFF )
      {
        if (_fe == -1) _fe = i;
        _le = i;
      }
      tq::add([=](){_loadPointers(_fe, _le, i+64, ondone);});
    });
  }

  void savePointers(int fe, int le)
  {
    auto buf = mkbuf(8);
    (*buf)[0] = fe & 0xFF;
    (*buf)[1] = (fe >> 8) & 0xFF;
    (*buf)[2] = (fe >> 16) & 0xFF;
    (*buf)[3] = 0x5a;
    (*buf)[4] = le & 0xFF;
    (*buf)[5] = (le >> 8) & 0xFF;
    (*buf)[6] = (le >> 16) & 0xFF;
    (*buf)[7] = 0x5a;
    rtc.writeSRAM(0, buf, 8, [=])
  }
  void loadPointers(std::function<void(int, int)> ondone)
  {
    printf("loading pointers\n");
    auto buf = mkbuf(8);
    rtc.readSRAM(0, buf, 8, [=](int status, buf_t buf)
    {
      if (status)
      {
        religion::enter_next_life(status);
      }
      int fe = -1;
      int le = -1;
      if ((*buf)[3] == 0x5a && (*buf)[7] != 0x5a)
      {
        fe = (*buf)[0];
        fe |= (*buf)[1] << 8;
        fe |= (*buf)[2] << 16;
        se = (*buf)[4];
        se |= (*buf)[5] << 8;
        se |= (*buf)[6] << 16;
      }
      _loadPointers(fe, se, 0, ondone);
    });

  }

  void doSync(std::function<void(void)> onDone)
  {
    rtc.getUnixTime([this, onDone](uint32_t t)
    {
      uint32_t n = storm::sys::now(storm::sys::SHIFT_16);
      lastRTCCTime = t - EPOCH;
      lastTicks = n;
      tq::add([=](){onDone();});
    });
  }
  void insertRecord(uint8_t b[4])
  {

  }
  uint32_t getRelTimestamp()
  {
    return getAbsTimestamp() - lastLogTime;
  }
  bool isReady()
  {
    return initialized;
  }
  uint32_t getAbsTimestamp()
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
  void incLastTimestamp(uint32_t t)
  {
    lastLogTime += t;
  }
  void setLastTimestamp(uint32_t t)
  {
    lastLogTime = t;
  }
  void logTimestamp()
  {
    uint32_t t = getAbsTimestamp();
    uint8_t b[4];
    b[0] = T_TIMESTAMP | ((t >> 24) & 0xF);
    b[1] = (t>>16) & 0xFF;
    b[2] = (t>>8) & 0xFF;
    b[3] = t & 0xFF;
    setLastTimestamp(t);
    insertRecord(b);
  }
  void logTempHumidity(uint16_t temp, uint16_t humidity)
  {
    uint32_t t = getRelTimestamp();
    if (t > 15) {
      logTimestamp();
      t = 0;
    }
    uint8_t b[4];
    b[0] = T_TEMPHUM | ((t << 2) & 0b111100) | ((humidity >> 10) & 0b11);
    b[1] = (humidity >> 2) & 0xFF;
    b[2] = ((humidity << 6) & 0b11000000) | ((temp >> 8) & 0b00111111);
    b[3] = temp;
    incLastTimestamp(t);
    insertRecord(b);
  }
  void logSettings(uint8_t backCool, uint8_t backHeat, uint8_t bottomHeat, uint8_t bottomCool)
  {
    uint32_t t = getRelTimestamp();
    if (t > 3) {
      logTimestamp();
      t = 0;
    }
    uint8_t b[4];
    b[0] = T_SETTING | (t << 6) | (bottomHeat >> 3);
    b[1] = (bottomHeat << 5) | ((backHeat >> 2) & 0b00011111);
    b[2] = (backHeat << 6) | ((bottomCool >> 1) & 0b00111111);
    b[3] = (bottomCool << 7) | (backCool & 0x7f);
    incLastTimestamp(t);
    insertRecord(b);
  }

private:

};
