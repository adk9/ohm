TGTS=\
	doctor\
	benchmarks\

all: $(TGTS)

libdwarf/libdwarf.a:
	make -C libdwarf

libunwind/libunwind-ptrace.a:
	make -C libunwind

libunwind/.libs/libunwind.a:
	make -C libunwind

libunwind/.libs/libunwind-x86_64.a:
	make -C libunwind

doctor: libdwarf/libdwarf.a libunwind/libunwind-ptrace.a libunwind/.libs/libunwind.a libunwind/.libs/libunwind-x86_64.a
	make -C src

benchmarks:
	make -C benchmarks

clean:
	make -C libdwarf clean
	make -C libunwind clean
	make -C src clean
	make -C benchmarks clean
