#the objective of this makefile is to compile using g++ specs
NEWLIB = ../newlib-2.2.0.20150623/bld
TOOLCH = /opt/gcc-arm-none-eabi-4_9-2015q2
CC       = arm-none-eabi-g++
CFLAGS   = -ffunction-sections -std=c++14 -fno-exceptions -fno-unwind-tables -fdata-sections -Os -g3 -fno-strict-aliasing -Wall -mfloat-abi=soft -mthumb -mcpu=cortex-m4 --specs=nano.specs --specs=nosys.specs
LDFLAGS  = -nostdlib -ffunction-sections -std=c++14 -fdata-sections -fno-strict-aliasing -Os -g3 -nostartfiles -T kernelpayload.ld -mfloat-abi=soft -mcpu=cortex-m4 -mthumb --specs=nano.specs --specs=nosys.specs
LDFLAGS += -L. -lrtt
LDFLAGS += -lstdc++ -lc -lgcc -Wl,--gc-sections
#we have libc.a rather than -lc above



tester: main.o interface.o libstorm.o libchair.o cmdline.o religion.o logfs.o control.o sht25.o comms.o mcp3425.o
	$(CC) -o chair.elf $^ $(LDFLAGS)
	arm-none-eabi-size chair.elf

all: clean tester

main.o: main.cc interface.h
	$(CC) -c $(CFLAGS) $<

interface.o: interface.c interface.h
	$(CC) -c $(CFLAGS) $<

cmdline.o: cmdline.cc cmdline.h
		$(CC) -c $(CFLAGS) $<

libstorm.o: libstorm.cc libstorm.h
	$(CC) -c $(CFLAGS) $<

libchair.o: libchair.cc libchair.h
	$(CC) -c $(CFLAGS) $<

religion.o: religion.cc religion.h
	$(CC) -c $(CFLAGS) $<

logfs.o: logfs.cc logfs.h
	$(CC) -c $(CFLAGS) $<

control.o: control.cc control.h
	$(CC) -c $(CFLAGS) $<

sht25.o: sht25.cc sht25.h
	$(CC) -c $(CFLAGS) $<

comms.o: comms.cc comms.h
	$(CC) -c $(CFLAGS) $<

mcp3425.o: mcp3425.cc mcp3425.h
	$(CC) -c $(CFLAGS) $<

install:
	sload program chair.elf

.PHONY: clean

clean:
	rm -f *.o tester
