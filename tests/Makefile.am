testsdir             = $(datarootdir)/doc/@PACKAGE@
tests_PROGRAMS       = whetdc counting fn-tracer nbody

if HAVE_MPI
tests_PROGRAMS      += mpi-counting
endif

nbody_SOURCES        = nbody.c
nbody_CFLAGS         = $(DWARF_CFLAGS) 

whetdc_SOURCES       = whetdc.c
whetdc_CFLAGS        = $(DWARF_CFLAGS) 

counting_SOURCES     = counting.c
counting_CFLAGS      = $(DWARF_CFLAGS) 

fn_tracer_SOURCES    = fn-tracer.c
fn_tracer_CFLAGS     = $(DWARF_CFLAGS) 

mpi_counting_SOURCES = mpi-counting.c
mpi_counting_CFLAGS  = $(DWARF_CFLAGS) $(MPI_CFLAGS)
mpi_counting_LDFLAGS = $(MPI_CLDFLAGS)

AM_CPPFLAGS          = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_LDFLAGS           = -static
