
#ifndef __LOGFS_H__
#define __LOGFS_H__

#include "religion.h"
#include "libchair.h"
#include "libstorm.h"
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
// === battery voltage k = battery_ok
// 1100 kttt
//  8t : 11 bit relative timestamp
//  16v : 24 bit battery voltage (msb first)
//
// === boot record v= version r=reset count R=religion marker
// 1101 vvvv
// rrrr rrRR
//  8R
//  8R

// 00xx : temp/hum telemetry
// 01xx : settings
// 10xx
// 1100 : battery voltage
// 1101 : boot record
// 1110 : timestamp
// 1111 : unalloc

//A single fragment is 73 bytes max UDP payload.
namespace firestorm
{

class LogFS
{
public:
  LogFS(firestorm::RTCC &rtc, std::function<void(void)> ondone);
  void logTempHumidityOccupancy(uint16_t temp, uint16_t humidity, bool occupancy);
  void logSettings(uint8_t backCool, uint8_t backHeat, uint8_t bottomCool, uint8_t bottomHeat);
  void logBatteryVoltageK(uint16_t v, bool battery_ok);
  void logBootRecord(uint16_t reset_count, uint32_t religion_rec);
  void factoryReset(std::function<void()>);
  void readRecord(std::function<void(buf_t)> onDone);
  void peekBatch(std::function<void(buf_t, uint32_t)> onDone);
  void releaseBatch(uint32_t addr, uint32_t ts, std::function<void()> onDone);
  uint32_t getAbsTimestamp();
  bool isReady();

  static constexpr const int EPOCH = 1420070400;
  static constexpr const int T_TIMESTAMP = 0b11100000;
  static constexpr const int T_TEMPHUM   = 0b00000000;
  static constexpr const int T_SETTING   = 0b01000000;
  static constexpr const int T_BATTERY   = 0b11000000;
  static constexpr const int T_BOOTREC   = 0b11010000;
  static constexpr const int AMAX = 0x780000;
  static constexpr const int BPAGE = 64;
  static constexpr const int RSIZE = 4;
  static constexpr const int FIRMWARE_VERSION = 4;
  static constexpr const int BMAX = AMAX/RSIZE; //Keep this a multiple of BATCHSIZE
  static constexpr const int QWATERMARK = 6;
  static constexpr const int BATCHSIZE = 16;
private:
  //Constants
  //midnight 1st jan 2015

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
  bool flashOpBusy = false;

  std::function<void(void)> oninitdone;

  std::queue<buf_t> insert_queue;

  int32_t read_ptr;
  int32_t write_ptr;
  buf_t temp_record;
  buf_t blank_record;

  //States
  //1 last block was free, looking for read ptr + write ptr
  //2 last block was occupied, looking for read ptr + write ptr
  //3 last block was free, looking for read ptr
  //4 last block was occupied, looking for read ptr
  //5 last block was free, looking for write ptr
  //6 last block was occupied, looking for write ptr
  void _loadPointers(int state, int i, int endi, std::function<void()> ondone);

  void savePointers(std::function<void()> ondone);

  void bootstrapPointersWithHint(int write_hint, int read_hint, std::function<void()> ondone);

  void bootstrapPointersWithNoHint(std::function<void()> ondone);

  void loadPointers(std::function<void()> ondone);

  void doSync(std::function<void(void)> onDone);

  void insertRecord(buf_t b);

  uint32_t getRelTimestamp();

  void schedule_queue();

  void incLastTimestamp(uint32_t t);

  void setLastTimestamp(uint32_t t);

  void logTimestamp();

};

}

#endif
