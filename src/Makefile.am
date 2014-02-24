bin_PROGRAMS   = ohm-doctor

ohm_doctor_SOURCES = dwarf-util.c doctor.c

AM_CPPFLAGS    = -D_POSIX_C_SOURCE=200809L -I/usr/include $(LUA_INCLUDE)
AM_CFLAGS      = -std=c99 $(OHM_PEDANTIC) $(OHM_W_ALL) $(OHM_W_ERROR)
AM_LDFLAGS     = $(LUA_LIB)

if DEBUG_BUILD
  ohm_doctor_CFLAGS = -g -O0
else
  ohm_doctor_CFLAGS = -O2
endif

if BUILD_LIBDWARF
  AM_CPPFLAGS += -I$(top_srcdir)/libdwarf/libdwarf/
  AM_LDFLAGS  += -L$(top_srcdir)/libdwarf/libdwarf/ -ldwarf
endif

if BUILD_LIBUNWIND
  AM_CPPFLAGS += -I$(top_srcdir)/libdwarf/libdwarf/ -I$(top_srcdir)/libunwind/include
  AM_LDFLAGS  += -L$(top_srcdir)/libunwind/src/ -lunwind -lunwind-ptrace -lunwind-x86_64
endif
