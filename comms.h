
#include <inttypes.h>

namespace firestorm
{
  class Comms
  {
  public:
    Comms(std::string remote, firestorm::LogFS &lg)
      :remote(remote), lg(lg)
    {
      sock = storm::UDPSocket::open(4039, [this](auto packet)
      {
        //Got reply packet
        uint32_t read_ptr = (uint32_t)packet->payload[0] + (((uint32_t)packet->payload[1]) << 8) + (((uint32_t)packet->payload[2]) << 16);
        uint32_t ts = (uint32_t)packet->payload[3] + (((uint32_t)packet->payload[4]) << 8) + (((uint32_t)packet->payload[5]) << 16) + (((uint32_t)packet->payload[6]) << 24);
        printf("!RELEASE %" PRIu32 "\n", read_ptr);
        this->lg.releaseBatch(read_ptr, ts, []{
          //printf("Released batch\n");
        });
      });
      storm::Timer::periodic(1*storm::Timer::SECOND, [this](auto)
      {
        if (!this->lg.isReady())
        {
          //printf("Log not ready\n");
          return;
        }
        this->lg.peekBatch([this](buf_t batch, uint32_t readptr)
        {
          if (batch == nullptr)
          {
            return;
          }
          auto obuf = mkbuf(67);
          (*obuf)[0] = (uint8_t)(readptr & 0xFF);
          (*obuf)[1] = (uint8_t)((readptr >> 8) & 0xFF);
          (*obuf)[2] = (uint8_t)((readptr >> 16) & 0xFF);
          memcpy(&((*obuf)[3]), &((*batch)[0]), 64);
          sock->sendto(this->remote, 4040, obuf, 67);
        });
      });
    }
  private:
    std::string remote;
    std::shared_ptr<storm::UDPSocket> sock;
    firestorm::LogFS &lg;
  };
}
