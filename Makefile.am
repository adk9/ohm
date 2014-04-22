EXTRA_DIST=README.md TODO

AUTOMAKE_OPTIONS= foreign
ACLOCAL_AMFLAGS = -I config

SUBDIRS      = tests
if BUILD_LIBDWARF
    SUBDIRS += libdwarf
endif
if BUILD_LIBUNWIND
    SUBDIRS += libunwind
endif
if ENABLE_LUAJIT
    SUBDIRS += luajit-2.0
endif
SUBDIRS     += src
