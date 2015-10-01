
#include <string>
#include <stdint.h>
#include "libstorm.h"
#include "libchair.h"

class CmdLine
{
public:
  CmdLine(firestorm::RTCC &rtcc);
  void fire(uint32_t num);
  void runcmd(char* cmd);
private:
  uint8_t buffer[80];
  uint8_t ptr;
  firestorm::RTCC &rtcc;
};
