
//YarnQ packet
// [seqno] [packet::]
// if (seqno == 255) then reset state


template <int osize, int isize>
class Yarnq
{
public:
  Yarnq(string dest, int port, int rate, std::function<void(int))
    :dest(dest), port(port), rate(rate)
  {
    //Enqueue a reset
    enqueue(mkbuf({0xFF}));
    storm::Timer::periodic(rate*storm::Timer::MILLISECOND, [this](auto t)
    {
      this->tick();
    });
    storm::UDPSocket::open(port, [this](auto packet)
    {
      if (packet[0] == 0xFF)
      {
        open = true;
        seqno = 0;
      }
      if (packet[0] == outq.front().get()[0])
        outq.pop();
      tick();
    });
  }

  bool enqueue(buf_t message);
  buf_t dequeue();
  int outbound_size();
private:
  void tick()
  {
    if (!outq.empty())
    {
      auto m = outq.front();
      //send m
    }
  }
  std::queue<buf_t, std::array<buf_t, osize>> outq;
  string dest;
  int port;
  int rate;
  bool open;
  int seqno;
};
