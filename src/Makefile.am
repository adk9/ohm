bin_PROGRAMS   = doctor

doctor_SOURCES = dwarf-util.c doctor.c

AM_CPPFLAGS    = -D_POSIX_C_SOURCE=200809L -I/usr/include $(LUA_INCLUDE)
AM_CFLAGS      = -std=c99
AM_LDFLAGS     = $(LUA_LIB)

if BUILD_LIBDWARF
  AM_CPPFLAGS += -I$(top_srcdir)/libdwarf/
  AM_LDFLAGS  += -L$(top_srcdir)/libdwarf/ -ldwarf
endif

if BUILD_LIBUNWIND
  AM_CPPFLAGS += -I$(top_srcdir)/libdwarf/ -I$(top_srcdir)/libunwind/include
  AM_LDFLAGS  += -L$(top_srcdir)/libunwind/src/ -lunwind -lunwind-ptrace -lunwind-x86_64
endif
