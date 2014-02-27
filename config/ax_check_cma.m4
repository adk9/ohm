# AX_CHECK_CMA

# check if cma support is wanted.
AC_DEFUN([AX_CHECK_CMA],[
    AC_ARG_ENABLE([cma], [AC_HELP_STRING([--enable-cma],
      [Enable Cross Memory Attach support @<:@default=no@:>@])],
      [enable_cma=yes], [enable_cma=no])

    AC_MSG_CHECKING([if user requested CMA support])
    if test "$enable_cma" = "yes" ; then
       	    AC_MSG_RESULT([yes])
	    AC_CHECK_HEADER([sys/uio.h])
            AC_CHECK_FUNC(process_vm_readv,  [cma_readv=1], [cma_readv=0])
            AC_CHECK_FUNC(process_vm_writev, [cma_writev=1], [cma_writev=0])
            AC_DEFINE_UNQUOTED([HAVE_CMA], [$cma_readv], [Needed CMA syscalls defined])
    else
       	    AC_MSG_RESULT([no])
    fi
])

