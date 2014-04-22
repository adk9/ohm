testsdir             = $(datarootdir)/doc/@PACKAGE@
tests_PROGRAMS       = whetdc counting mpi-counting

whetdc_SOURCES       = whetdc.c
whetdc_CFLAGS        = $(DWARF_CFLAGS) 

counting_SOURCES     = counting.c
counting_CFLAGS      = $(DWARF_CFLAGS) 

mpi_counting_SOURCES = mpi-counting.c
mpi_counting_CFLAGS  = $(MPI_CFLAGS)
mpi_counting_LDFLAGS = $(MPI_CLDFLAGS)

AM_CPPFLAGS          = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_LDFLAGS           = -static
