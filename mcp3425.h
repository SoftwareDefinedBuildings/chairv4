#ifndef __MCP3425_H__
#define __MCP3425_H__

#include "libstorm.h"
#include "libchair.h"

class MCP3425
{
public:
  MCP3425();
  void sample(std::function<void(uint8_t, uint16_t)> cb);
private:
  void configure();
  bool configured;
};

#endif
