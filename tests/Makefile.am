testsdir            = $(datarootdir)/doc/@PACKAGE@
tests_PROGRAMS      = whetdc counting

whetdc_SOURCES      = whetdc.c
counting_SOURCES    = counting.c

whetdc_CFLAGS       = $(DWARF_CFLAGS) 
counting_CFLAGS     = $(DWARF_CFLAGS) 

AM_CPPFLAGS         = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_LDFLAGS          = -static
