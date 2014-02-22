# Copyright (c) 2014, Abhishek Kulkarni
# All rights reserved. This software may be modified
# and distributed under the terms of the BSD license.
# See the COPYING file for details.

AC_PREREQ([2.65])
AC_INIT([ohm], [1.0], [adkulkar@indiana.edu])
AC_CONFIG_SRCDIR([src/doctor.c])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.2.6])
LT_INIT

AC_LANG([C])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([elf], [elf_begin])
AC_CHECK_LIB([m], [pow])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([strerror])

# libelf
AC_SEARCH_LIBS([elf32_getehdr], [elf], [], [AC_MSG_ERROR([libelf not found])])
AC_CHECK_HEADERS([elf.h libelf.h], [found_libelf_hdr=yes; break;])
AS_IF([test "x$found_libelf_hdr" != "xyes"],
      [AC_MSG_ERROR([Unable to find libelf.h])])

# libdwarf
AC_SEARCH_LIBS([dwarf_tag], [dwarf], [found_libdwarf=yes],
               [AC_MSG_NOTICE([libdwarf not found])])
AC_CHECK_HEADERS([dwarf.h libdwarf.h], [found_libdwarf_hdr=yes; break;])
AS_IF([test "x$found_libdwarf" != "xyes" -o "x$found_libdwarf_hdr" != "xyes"],
      [AC_MSG_NOTICE([Unable to find libdwarf.h])
       AC_CONFIG_SUBDIRS([libdwarf])
       build_libdwarf=yes],
      [build_libdwarf=no])
AM_CONDITIONAL([BUILD_LIBDWARF], [test "x$build_libdwarf" == "xyes"])

# libunwind
AC_SEARCH_LIBS([unw_create_addr_space], [unwind unwind-ptrace], [found_libunwind=yes],
               [AC_MSG_NOTICE([libunwind not found])])
AC_CHECK_HEADERS([libunwind-ptrace.h], [found_libunwind_hdr=yes; break;])
AS_IF([test "x$found_libunwind" != "xyes" -o "x$found_libunwind_hdr" != "xyes"],
      [AC_MSG_NOTICE([Unable to find libunwind headers])
       AC_CONFIG_SUBDIRS([libunwind])
       build_libunwind=yes],
      [build_libunwind=no])
AM_CONDITIONAL([BUILD_LIBUNWIND], [test "x$build_libunwind" == "xyes"])

# detect lua
AX_PROG_LUA([5.0],[],[
    AX_LUA_HEADERS([
        AX_LUA_LIBS([
            AC_DEFINE([HAVE_LUA], [1], [Found Lua])
            HAVE_LUA=yes
        ],[AC_MSG_ERROR([cannot find lua libs])])
    ],[AC_MSG_ERROR([cannot find lua headers])])
],[AC_MSG_ERROR([cannot find lua >= 5.0])])

AC_CONFIG_FILES([Makefile
                 benchmarks/Makefile
                 src/Makefile])
AC_OUTPUT
