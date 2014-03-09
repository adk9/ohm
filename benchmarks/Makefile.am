benchmarksdir       = $(datarootdir)/doc/@PACKAGE@
benchmarks_PROGRAMS = whetdc counting

whetdc_SOURCES      = whetdc.c
counting_SOURCES    = counting.c

AM_CPPFLAGS         = $(DWARF_CFLAGS) -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS           = -std=c99
AM_LDFLAGS          = -static
