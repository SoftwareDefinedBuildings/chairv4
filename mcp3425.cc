
#include "mcp3425.h"
#include "religion.h"

/*
class MCP3425
{
public:
  MCP3425();
  sample(std::function<void(uint8_t, uint16_t)> cb);
private:
  configure();
};
*/

MCP3425::MCP3425()
: configured(false)
{
  tq::add([=]()
  {
    this->configure();
  });
}

void MCP3425::sample(std::function<void(uint8_t, uint16_t)> cb)
{
  if(!configured)
  {
    cb(1, 0);
    return;
  }
  auto b = mkbuf(2);
  storm::i2c::lock.acquire([=]
  {
    storm::i2c::read(storm::i2c::external(0b11010000), storm::i2c::START | storm::i2c::STOP, b, 2, [=](auto st, auto)
    {
      storm::i2c::lock.release();
      if (st)
      {
        cb(st, 0);
        return;
      }
      uint16_t v = (*b)[0];
      v <<= 8;
      v |= (*b)[1];
      cb(0, v);
    });
  });
}

void MCP3425::configure()
{
  storm::i2c::lock.acquire([=]
  {
    //Configure for continuous sampling, 15SPS (16 bit), 1x gain
    auto b = mkbuf({0b10011000});
    storm::i2c::write(storm::i2c::external(0b11010000), storm::i2c::START | storm::i2c::STOP, b, 1, [=](auto st, auto)
    {
      if (st)
      { //This should not happen
        religion::enter_next_life(st);
      }
      configured = true;
      storm::i2c::lock.release();
    });
  });
}
