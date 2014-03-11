# AX_CHECK_XPMEM

# check if xpmem support is wanted.
AC_DEFUN([AX_CHECK_XPMEM],[
    AC_ARG_ENABLE([xpmem], [AC_HELP_STRING([--enable-xpmem],
      [Enable XPMEM support @<:@default=no@:>@])],
      [enable_xpmem=yes], [enable_xpmem=no])

    AC_MSG_CHECKING([if user requested XPMEM support])
    if test "$enable_xpmem" = "yes" ; then
       	    AC_MSG_RESULT([yes])
            PKG_CHECK_MODULES([XPMEM], [cray-xpmem],
              [AC_DEFINE([HAVE_XPMEM], [1], [Use XPMEM])])
    else
       	    AC_MSG_RESULT([no])
    fi
])
