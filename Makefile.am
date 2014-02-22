EXTRA_DIST=README.md TODO

AUTOMAKE_OPTIONS= foreign
ACLOCAL_AMFLAGS = -I config

SUBDIRS      = benchmarks
if BUILD_LIBDWARF
    SUBDIRS += libdwarf
endif
if BUILD_LIBUNWIND
    SUBDIRS += libunwind
endif
SUBDIRS     += src
