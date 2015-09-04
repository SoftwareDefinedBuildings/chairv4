#the objective of this makefile is to compile using g++ specs
NEWLIB = ../newlib-2.2.0.20150623/bld
TOOLCH = /opt/gcc-arm-none-eabi-4_9-2015q2
CC       = arm-none-eabi-g++
CFLAGS   = -ffunction-sections -std=c++14 -fdata-sections -fno-strict-aliasing -g3 -Wall -mfloat-abi=soft -mthumb -mcpu=cortex-m4 --specs=nano.specs --specs=nosys.specs
LDFLAGS  = -nostdlib -ffunction-sections -std=c++14 -fdata-sections -fno-strict-aliasing -g3  -nostartfiles -T kernelpayload.ld -mfloat-abi=soft -mcpu=cortex-m4 -mthumb --specs=nano.specs --specs=nosys.specs
LDFLAGS += -L. -lrtt
LDFLAGS += -lstdc++ libc.a -lgcc -Wl,--gc-sections
#

all: clean tester

tester: main.o interface.o libstorm.o libchair.o
	$(CC) -o chair.elf $^ $(LDFLAGS)

main.o: main.cc interface.h
	$(CC) -c $(CFLAGS) $<

interface.o: interface.c interface.h
	$(CC) -c $(CFLAGS) $<

libstorm.o: libstorm.cc libstorm.h
	$(CC) -c $(CFLAGS) $<

libchair.o: libchair.cc libchair.h
	$(CC) -c $(CFLAGS) $<

install:
	sload program chair.elf

.PHONY: clean

clean:
	rm -f *.o tester
