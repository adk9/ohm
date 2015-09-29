bin_PROGRAMS   = ohmd

ohmd_SOURCES   = dwarf-util.c lua-util.c types.c funcvars.c probes.c ohmd.c

if ENABLE_XPMEM
ohmd_SOURCES  += xpmem.c
endif

AM_CPPFLAGS    = $(XPMEM_CFLAGS) -D_POSIX_C_SOURCE=200809L -I/usr/include -I$(top_srcdir)/include
AM_LDFLAGS     = -static
ohmd_CFLAGS    = -Wno-error=format $(OHM_PEDANTIC) $(OHM_W_ALL) $(OHM_W_ERROR) -O3
ohmd_LDADD     = $(XPMEM_LIBS)

if HAVE_MPI
ohmd_CFLAGS   += $(MPI_CFLAGS)
ohmd_LDADD    += $(MPI_CLDFLAGS)
endif

if DEBUG_BUILD
  ohmd_CFLAGS += -g -O0
else
  ohmd_CFLAGS += -O2
endif

if BUILD_LIBDWARF
  AM_CPPFLAGS += -I$(top_srcdir)/libdwarf/libdwarf
  ohmd_LDADD  += $(top_srcdir)/libdwarf/libdwarf/libdwarf.a
endif

if BUILD_LIBUNWIND
  AM_CPPFLAGS += -I$(top_srcdir)/libdwarf/libdwarf -I$(top_srcdir)/libunwind/include
  ohmd_LDADD  += $(top_srcdir)/libunwind/src/libunwind.la
  ohmd_LDADD  += $(top_srcdir)/libunwind/src/libunwind-ptrace.la
  ohmd_LDADD  += $(top_srcdir)/libunwind/src/libunwind-$(arch).la
endif

if ENABLE_LUAJIT
  AM_CPPFLAGS += -I$(top_srcdir)/luajit-2.0/src
  ohmd_LDADD  += $(top_srcdir)/luajit-2.0/src/libluajit.a
else
  AM_CPPFLAGS += $(LUA_INCLUDE)
  AM_LDFLAGS  += $(LUA_LIB)
endif

if ENABLE_XPMEM
lib_LTLIBRARIES = libohm_xpmem.la

libohm_xpmem_la_SOURCES  = ohm_xpmem.c
libohm_xpmem_la_LIBADD   = $(XPMEM_LIBS)
endif
