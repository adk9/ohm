SYSNAME:=${shell uname}
SYSNAME!=uname
LUA=lua-5.2.2

CFLAGS=-Wall -g -I. -Ilibdwarf/libdwarf -Ilibunwind/include/ $(shell pkg-config --cflags ${LUA})
LFLAGS=-L. $(shell pkg-config --libs ${LUA})
DWARFLDIR=libdwarf/libdwarf
UNWINDLDIR=libunwind/src/.libs
CC=gcc

PROG=\
	whetdc\
	doctor\

all: $(PROG)

$(DWARFLDIR)/libdwarf.a:
	make -C libdwarf/libdwarf

$(UNWINDLDIR)/libunwind-ptrace.a:
	make -C libunwind

$(UNWINDLDIR)/.libs/libunwind.a:
	make -C libunwind

$(UNWINDLDIR)/.libs/libunwind-x86_64.a:
	make -C libunwind

whetdc: whetdc.o
	$(CC) -o whetdc $(CFLAGS) whetdc.o $(LFLAGS) -lm

whetdc.o: whetdc.c 
	$(CC) $(CFLAGS) -DPRINTOUT -c whetdc.c

doctor: $(DWARFLDIR)/libdwarf.a $(UNWINDLDIR)/libunwind-ptrace.a $(UNWINDLDIR)/.libs/libunwind.a $(UNWINDLDIR)/.libs/libunwind-x86_64.a dwarf-util.o doctor.o
	$(CC) -o doctor $(CFLAGS) dwarf-util.o doctor.o $(DWARFLDIR)/libdwarf.a $(UNWINDLDIR)/libunwind-ptrace.a $(UNWINDLDIR)/libunwind-x86_64.a $(UNWINDLDIR)/libunwind.a $(LFLAGS) -ldl -lelf -lm

symbols: symbols.o
	$(CC) -o symbols $(CFLAGS) symbols.o $(LFLAGS) -lbfd

%.o: %.c 
	$(CC) $(CFLAGS) -c $*.c

clean:
	rm -f *.o *~ $(PROG) core.*

nuke:
	make -C libdwarf/libdwarf clean
	make -C libunwind clean
	make clean
