

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
// === temp/humidity
// 00tt tthh
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

  //midnight 1st jan 2015
  const int EPOCH = 1420070400;
  const int T_TIMESTAMP = 0b11100000;
  const int T_TEMPHUM   = 0b00000000;
  const int T_SETTING   = 0b01000000;

  const int AMAX = 0x780000;
  const int BMAX = 0x1e0000;
  const int BPAGE = 64;

  firestorm::RTCC &rtc;
  bool ready = false;
  std::function<void(void)> const& ondone;
  uint32_t lastRTCCTime;
  uint32_t lastTicks;
  uint32_t lastLogTime;

  uint32_t read_block;
  uint32_t write_block;

  LogFS(firestorm::RTCC &rtc, std::function<void(void)> const& ondone)
    :rtc(rtc), ondone(ondone)
  {
    ready = false;
    loadPointers();
    doSync(ondone);
  }

  void loadPointers()
  {
    int fe = -1;
    int le = -1;
    uint8_t buf [4];
    for (i = 0; i < BMAX; i+= 8*BPAGE)
    {
      storm::
    }
  }
  void doSync(std::function<void(void)> onDone)
  {
    rtc.getUnixTime([](uint32_t t)
    {
      uint32_t n = storm::sys::now(SHIFT_16);
      lastRCCTime = t;
      lastTicks = n;
      onDone();
    });
  }
  uint32_t getRelTimestamp()
  {

  }
  bool isReady()
  {
    return ready;
  }
  uint32_t getAbsTimestamp()
  {

  }
  void incLastTimestamp(uint32_t t)
  {

  }
  void setLastTimestamp(uint32_t t)
  {

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
    b[2] = ((humidity << 6) & 0b11000000) | ((temperature >> 8) & 0b00111111);
    b[3] = temperature;
    incLastTimestamp(t);
    insertRecord(b);
  }
  void logSettings(uin8_t backCool, uint8_t backHeat, uint8_t bottomHeat, uint8_t bottomCool)
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
