
#include "religion.h"
#include <stdint.h>
#include <stdio.h>
#include "libstorm.h"

static firestorm::RTCC *target_rtc = nullptr;

void religion::set_sram(firestorm::RTCC *rtc)
{
  target_rtc = rtc;
}
void religion::set_record(uint32_t v)
{
  auto buf = mkbuf(4);
  (*buf)[0] = v >> 24;
  (*buf)[1] = v >> 16;
  (*buf)[2] = v >> 8;
  (*buf)[3] = v;
  //SRAM 0-7 is flash log
  //SRAM 8-15 is settings
  //SRAM 16-19 is religion
  if (target_rtc != nullptr)
  {
    target_rtc->writeSRAM(16, buf, 4, [=](int status){
    });
  }
}
void religion::enter_next_life(uint32_t memory)
{
  uint32_t caller = (uint32_t) __builtin_return_address(0);
  set_record(caller);

  while(1) //WDT will reset for us
  {
    printf("religion 0x%08x 0x%08x\n", (unsigned int)caller, (unsigned int)memory);
  }
  //TODO
}
