# Copyright (c) 2014, Abhishek Kulkarni
# All rights reserved. This software may be modified
# and distributed under the terms of the BSD license.
# See the COPYING file for details.

AC_PREREQ([2.65])
AC_INIT([ohm], [1.0], [adkulkar@indiana.edu])
AC_CONFIG_SRCDIR([src/ohmd.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([config])
AC_CONFIG_AUX_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.2.6])
LT_INIT

AC_LANG([C])

CFLAGS="-O0"
# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Check for the MPI compiler.
LX_FIND_MPI
AM_CONDITIONAL([HAVE_MPI], [test "x$have_C_mpi" == xyes])

# Checks for libraries.
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([elf], [elf_begin])
AC_CHECK_LIB([m], [pow])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h stdbool.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([strerror])

# Set architecture
AC_DEFUN([SET_ARCH],[
    AS_CASE([$1],
        [arm*],[$2=arm],
        [i?86],[$2=x86],
        [hppa*],[$2=hppa],
        [mips*],[$2=mips],
        [powerpc*],[$2=ppc$ppc_bits],
        [sh*],[$2=sh],
        [amd64],[$2=x86_64],
        [$2=$1])
]) dnl SET_ARCH

SET_ARCH([$build_cpu],[build_arch])
SET_ARCH([$host_cpu],[host_arch])

AC_MSG_CHECKING([for build architecture])
AC_MSG_RESULT([$build_arch])
AC_MSG_CHECKING([for host architecture])
AC_MSG_RESULT([$host_arch])
AC_MSG_CHECKING([for host operating system])
AC_MSG_RESULT([$host_os])

arch="$host_arch"
AC_SUBST(arch)

# Check for flag to force dwarf-3.
DWARF_CFLAGS=""
AX_CHECK_COMPILE_FLAG([-gdwarf-3], [DWARF_CFLAGS="-gdwarf-3"], [DWARF_CFLAGS="-g"], [])
AC_SUBST([DWARF_CFLAGS])

# libelf
AC_SEARCH_LIBS([elf32_getehdr], [elf], [], [AC_MSG_ERROR([libelf not found])])
AC_CHECK_HEADERS([elf.h libelf.h], [found_libelf_hdr=yes; break;])
AS_IF([test "x$found_libelf_hdr" != "xyes"],
      [AC_MSG_ERROR([Unable to find libelf.h])])

# libdwarf
AC_ARG_ENABLE([internal-libdwarf],
  [AS_HELP_STRING([--enable-internal-libdwarf], [Enable Internal libdwarf @<:@default=no@:>@])],
  [enable_internal_libdwarf=yes], [enable_internal_libdwarf=no])
AC_SEARCH_LIBS([dwarf_tag], [dwarf], [found_libdwarf=yes],
               [AC_MSG_NOTICE([libdwarf not found])])
AC_CHECK_HEADERS([dwarf.h libdwarf.h], [found_libdwarf_hdr=yes; break;])
AS_IF([test "x$found_libdwarf" != "xyes" -o "x$found_libdwarf_hdr" != "xyes" -o "x$enable_internal_libdwarf" == "xyes"],
      [AC_MSG_NOTICE([Building internal libdwarf...])
       AC_CONFIG_SUBDIRS([libdwarf])
       build_libdwarf=yes],
      [build_libdwarf=no])
AM_CONDITIONAL([BUILD_LIBDWARF], [test "x$build_libdwarf" == "xyes"])

# libunwind
AC_ARG_ENABLE([internal-libunwind],
  [AS_HELP_STRING([--enable-internal-libunwind], [Enable Internal libunwind @<:@default=no@:>@])],
  [enable_internal_libunwind=yes], [enable_internal_libunwind=no])
AC_SEARCH_LIBS([unw_backtrace], [unwind],
	       [found_libunwind=yes
	       LIBS="$LIBS -lunwind -lunwind-generic"],
               [AC_MSG_NOTICE([libunwind not found])])
AC_SEARCH_LIBS([_UPT_access_fpreg], [unwind-ptrace], [found_libunwind=yes],
               [AC_MSG_NOTICE([libunwind-ptrace not found])])
AC_CHECK_HEADER([libunwind.h], [found_libunwind_hdr=yes; break;])
AS_IF([test "x$found_libunwind" != "xyes" -o "x$found_libunwind_hdr" != "xyes" -o "x$enable_internal_libunwind" == "xyes"],
      [AC_MSG_NOTICE([Building internal libunwind...])
       AC_CONFIG_SUBDIRS([libunwind])
       build_libunwind=yes],
      [build_libunwind=no])
AM_CONDITIONAL([BUILD_LIBUNWIND], [test "x$build_libunwind" == "xyes"])

# detect lua
AX_PROG_LUA([5.0],[],[
    AX_LUA_HEADERS([
        AX_LUA_LIBS([have_lua=1],
                    [AC_MSG_ERROR([cannot find lua libs])
                     have_lua=0])
    ],[AC_MSG_ERROR([cannot find lua headers])])
],[AC_MSG_ERROR([cannot find lua >= 5.0])])
AC_DEFINE([HAVE_LUA], [$have_lua], [Found Lua])

# detect cross-memory attach support
AX_CHECK_CMA

# check if XPMEM is present
AX_CHECK_XPMEM

# enable luajit
AC_ARG_ENABLE([luajit],
  [AS_HELP_STRING([--enable-luajit], [Enable LuaJIT support @<:@default=no@:>@])],
  [enable_luajit=yes], [enable_luajit=no])
AS_IF([test "x$enable_luajit" != xno],
  [AC_SEARCH_LIBS([luaL_newstate], [luajit], [have_luajit=yes],
                 [AC_MSG_NOTICE([libluajit not found])])
   PKG_CHECK_MODULES([luajit], [luajit], [have_luajit=yes], [have_luajit=no])
   AS_IF([test "x$have_luajit" != xno],
     [LUA_INCLUDE="${luajit_CFLAGS}"
     LUA_LIB="${luajit_LIBS}"
     AC_SUBST(LUA_INCLUDE)
     AC_SUBST(LUA_LIB)])])
AM_CONDITIONAL([ENABLE_LUAJIT], [test "x$enable_luajit" == "xyes"])

# More arguments for debug builds.
AC_ARG_ENABLE([pedantic],
  [AS_HELP_STRING([--enable-pedantic], [Enable -pedantic @<:@default=no@:>@])],
  [enable_pedantic=yes], [enable_pedantic=no])
AS_IF([test "x$enable_pedantic" != xno],
  [AC_SUBST([OHM_PEDANTIC], [-pedantic])])

AC_ARG_ENABLE([wall],
  [AS_HELP_STRING([--enable-wall], [Enable -Wall @<:@default=yes@:>@])],
  [], [enable_wall=yes])
AS_IF([test "x$enable_wall" != xno],
  [AC_SUBST([OHM_W_ALL], [-Wall])])

AC_ARG_ENABLE([werror],
  [AS_HELP_STRING([--enable-werror], [Enable -Werror @<:@default=yes@:>@])],
  [], [enable_werror=yes])
AS_IF([test "x$enable_werror" != xno],
  [AC_SUBST([OHM_W_ERROR], [-Werror])])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [Enable debug code @<:@default=no@:>@])],
  [], [enable_debug=no])
AS_IF([test "x$enable_debug" != xno],
  [AC_DEFINE([ENABLE_DEBUG], [1], [Enable debugging stuff])])
AM_CONDITIONAL(DEBUG_BUILD, test x"$enable_debug" = xyes)

AC_CONFIG_FILES([Makefile
                 tests/Makefile
                 src/Makefile])
AC_OUTPUT
