SYSNAME:=${shell uname}
SYSNAME!=uname
LUA=lua5.1

DWARFDIR=../libdwarf
UNWINDDIR=../libunwind
UNWINDLDIR=$(UNWINDDIR)/src/.libs

CFLAGS=-Wall -g -I. -I$(DWARFDIR) -I$(UNWINDDIR)/include $(shell pkg-config --cflags ${LUA})
LFLAGS=-L. $(shell pkg-config --libs ${LUA})

all: doctor

doctor: dwarf-util.o doctor.o
	$(CC) -o doctor $(CFLAGS) dwarf-util.o doctor.o $(DWARFDIR)/libdwarf.a $(UNWINDLDIR)/libunwind-ptrace.a $(UNWINDLDIR)/libunwind-x86_64.a $(UNWINDLDIR)/libunwind.a $(LFLAGS) -ldl -lelf -lm

%.o: %.c 
	$(CC) $(CFLAGS) -c $*.c

clean:
	rm -f *.o *~ doctor core.*

