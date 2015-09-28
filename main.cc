#include <stdio.h>
#include "interface.h"
#include <functional>
#include "libstorm.h"
#include "SEGGER_RTT.h"
#include <malloc.h>
#include "libchair.h"


using namespace storm;

int main()
{
  printf("booting payload\n");

  //Posting a task
  tq::add([]
  {
    printf("This is like posting a task in TinyOS\n");
  });

  //An example of capturing a buffer
  auto buffer = mkbuf({2,3,4});
  tq::add([buf = move(buffer)]
  {
    printf("The buffer has %d %d %d\n",
         (*buf)[0], (*buf)[1], (*buf)[2]);
  });


  //Using some library classes
  auto rtc = firestorm::RTCC();
  auto tempsensor = firestorm::TMP006();

  int counter = 0;

  Timer::periodic(1*Timer::SECOND, [&]
  {
      tempsensor.getDieTemp([&counter](auto v)
      {
        printf("Got temperature %d mC\n", (int)(v*1000));
        counter++;
        if (counter&1) printf("\n");
      });
      rtc.getStringTime([&counter](auto t)
      {
        printf("Got time %s\n", t.c_str());
        counter++;
        if (counter&1) printf("\n");
      });
  });

  auto b = mkbuf({4, 41});
  storm::flash::write(0, b, 2, [](auto a, auto b)
  {
    printf("Wrote flash at addr 0\n");
    auto b2 = mkbuf(3);
    storm::flash::read(0, b2, 3, [](auto a, auto b2)
    {
      printf("read 3 bytes: %d %d %d\n", (*b2)[0], (*b2)[1], (*b2)[2]);
    });
  });

  tq::scheduler();
}
