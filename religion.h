
#ifndef __RELIGION_H__
#define __RELIGION_H__

#include <stdint.h>
#include "libchair.h"
namespace religion
{
  void enter_next_life(uint32_t memory);
  void set_record(uint32_t v);
  void set_sram(firestorm::RTCC *rtc);
}

#endif
