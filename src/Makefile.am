bin_PROGRAMS   = ohm-doctor

ohm_doctor_SOURCES = dwarf-util.c doctor.c

AM_CPPFLAGS        = $(XPMEM_CFLAGS) -D_POSIX_C_SOURCE=200809L -I/usr/include 
AM_LDFLAGS         = -static
ohm_doctor_CFLAGS  = -Wno-error=format $(OHM_PEDANTIC) $(OHM_W_ALL) $(OHM_W_ERROR)
ohm_doctor_LDADD   = $(XPMEM_LIBS) 

if DEBUG_BUILD
  ohm_doctor_CFLAGS += -g -O0
else
  ohm_doctor_CFLAGS += -O2
endif

if BUILD_LIBDWARF
  AM_CPPFLAGS      += -I$(top_srcdir)/libdwarf/libdwarf
  ohm_doctor_LDADD += $(top_srcdir)/libdwarf/libdwarf/libdwarf.a
endif

if BUILD_LIBUNWIND
  AM_CPPFLAGS += -I$(top_srcdir)/libdwarf/libdwarf -I$(top_srcdir)/libunwind/include
  ohm_doctor_LDADD += $(top_srcdir)/libunwind/src/libunwind.la
  ohm_doctor_LDADD += $(top_srcdir)/libunwind/src/libunwind-ptrace.la
  ohm_doctor_LDADD += $(top_srcdir)/libunwind/src/libunwind-$(arch).la
endif

if ENABLE_LUAJIT
  AM_CPPFLAGS      += -I$(top_srcdir)/luajit-2.0/src
  ohm_doctor_LDADD += $(top_srcdir)/luajit-2.0/src/libluajit.a
else
  AM_CPPFLAGS += $(LUA_INCLUDE)
  AM_LDFLAGS  += $(LUA_LIB)
endif

lib_LTLIBRARIES          = libohm_xpmem.la

libohm_xpmem_la_SOURCES  = ohm_xpmem.c
libohm_xpmem_la_LIBADD   = $(XPMEM_LIBS)