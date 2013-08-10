SYSNAME:=${shell uname}
SYSNAME!=uname

PROG=\
	whetdc\

all: $(PROG)

whetdc: whetdc.o
	$(CC) -o whetdc $(CFLAGS) whetdc.o $(LFLAGS) -lm

whetdc.o: whetdc.c 
	$(CC) $(CFLAGS) -DPRINTOUT -c whetdc.c

%.o: %.c 
	$(CC) $(CFLAGS) -c $*.c

clean:
	rm -f *.o *~ $(PROG) core.*

