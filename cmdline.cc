
#include <stdio.h>
#include "interface.h"
#include <functional>
#include "libstorm.h"
#include <malloc.h>
#include "cmdline.h"
#include <string.h>
#include "libchair.h"

using namespace storm;

void _cmdline_cb(void *r, int32_t num)
{
  static_cast<CmdLine*>(r)->fire(num);
}

CmdLine::CmdLine(firestorm::RTCC &rtcc, firestorm::LogFS &log)
 : rtcc(rtcc), log(log)
{
  ptr = 0;
  fire(0);
}
void CmdLine::runcmd(char* cmd)
{
  char buf [80];
  printf("got cmd '%s'\n", cmd);
  strncpy(&buf[0], cmd, 80);
  char action [80];
  sscanf(buf, "%s\n", &action[0]);
  if (strcmp(&action[0],"stime")==0)
  {
    int month, day, hour, min, second;
    sscanf(buf, "%*s %d %d %d %d %d\n", &month, &day, &hour, &min, &second);
    firestorm::rtcc_time_t nt;
    nt.month = month;
    nt.day = day;
    nt.hour = hour;
    nt.min = min;
    nt.sec = second;
    nt.year = 15;
    rtcc.setTime(nt, [](int status)
    {
      printf("time set status=%d (%s)\n", status, status?"ERROR":"OKAY");
    });
  }
  else if (strcmp(&action[0],"gtime")==0)
  {
    rtcc.getRTCTime([](firestorm::rtcc_time_t t)
    {
      printf("time: 20%02d/%d/%d %d:%d:%d\n", t.year, t.month, t.day, t.hour, t.min, t.sec);
    });
  }
  else if (strcmp(&action[0],"eraselog")==0)
  {
    log.factoryReset([]{
      printf("Reset done\n");
    });
  }
  else if (strcmp(&action[0],"_poplog")==0)
  {
    log.readRecord([](auto b)
    {
      if (b)
      {
        printf("Read: %02x %02x %02x %02x\n", (*b)[0], (*b)[1], (*b)[2], (*b)[3]);
      }
      else
      {
        printf("Nothing in log\n");
      }
    });
  }
  else if (strcmp(&action[0],"_ins")==0)
  {
    log.logTempHumidityOccupancy(32,85,0);
  }
  else
  {
    printf("possible actions: \n");
    printf(" gtime\n");
    printf(" stime MM DD HH MM SS\n");
    printf(" eraselog (also serial number)\n");
    printf(" _poplog\n");
    printf(" _ins\n");
  }
}
void CmdLine::fire(uint32_t num)
{
again:
  for (uint32_t i = 0; i < num; i++)
  {
    if (buffer[i + ptr] == '\n')
    {
      buffer[i+ptr] = 0;
      runcmd((char*)&buffer[0]);
      int oldptr = ptr;
      ptr = 0;
      i++;
      int oldidx = i;
      num -= oldidx;
      if (i == num)
        break;
      for (; i < num; i++)
      {
        buffer[ptr++] = buffer[oldptr+i];
      }
      goto again;
    }
  }
  ptr += num;
  if (ptr == sizeof(buffer))
  { //discard the buffer if no command found in 80 chars
    ptr = 0;
  }
  k_read_async(0, &buffer[ptr], sizeof(buffer)-ptr, _cmdline_cb, this);
}
