
#include <string>
#include <stdint.h>
#include "libstorm.h"
#include "libchair.h"
#include "logfs.h"

class CmdLine
{
public:
  CmdLine(firestorm::RTCC &rtcc, firestorm::LogFS &log);
  void fire(uint32_t num);
  void runcmd(char* cmd);
private:
  uint8_t buffer[80];
  uint8_t ptr;
  firestorm::RTCC &rtcc;
  firestorm::LogFS &log;
};
