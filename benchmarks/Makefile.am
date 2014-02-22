benchmarksdir       = $(datarootdir)/doc/@PACKAGE@
benchmarks_PROGRAMS = whetdc

whetdc_SOURCES      = whetdc.c

AM_CPPFLAGS         = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS           = -std=c99
AM_LDFLAGS          = -static
