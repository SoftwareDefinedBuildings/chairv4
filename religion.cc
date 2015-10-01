
#include "religion.h"
#include <stdint.h>
#include <stdio.h>
#include "libstorm.h"

void religion::enter_next_life(uint32_t memory)
{
  uint32_t caller = (uint32_t) __builtin_return_address(0);
  while(1)
  {
    printf("religion 0x%08x 0x%08x\n", caller, memory);
  }
  //TODO
}
