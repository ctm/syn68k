# $Id: configure.ac 72 2004-12-28 15:57:12Z ctm $

AC_INIT(syn68k, 1.0, ctm@ardi.com)
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([syngen/main.c])
AC_CONFIG_HEADERS([include/config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_RANLIB
AM_PROG_AS

AC_CHECK_PROG([PERL], [perl], [perl])
AC_ARG_VAR([PERL], [perl for preening assembly])

#SUBDIRS="syngen runtime test profile" 
#configdirs="syngen runtime test profile" 

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AC_MSG_CHECKING([(non-cross) C compiler for building tools])
if test "$cross_compiling" = "yes"; then
  CC_FOR_BUILD="${CC_FOR_BUILD-gcc}"
else
  CC_FOR_BUILD="${CC_FOR_BUILD-$CC}"
fi
AC_MSG_RESULT([$CC_FOR_BUILD])
AC_ARG_VAR(CC_FOR_BUILD,[C compiler for building tools])

CFLAGS_FOR_BUILD="${CFLAGS_FOR_BUILD-}"
AC_ARG_VAR(CFLAGS_FOR_BUILD,[C flags for building tools])

AC_MSG_CHECKING([Syn68k C flags (TODO: FIXME)])
SYN68K_CFLAGS="${SYN68K_CFLAGS--O2 -fomit-frame-pointer}"
AC_MSG_RESULT([$SYN68K_CFLAGS])
AC_ARG_VAR(SYN68K_CFLAGS,[Syn68k C flags (TODO: FIXME)])

AC_MSG_CHECKING([cleanup script])
CLEANUP="${CLEANUP-./i486-cleanup.pl}"
AC_MSG_RESULT([$CLEANUP])
AC_ARG_VAR(CLEANUP,[cleanup script])

AC_MSG_CHECKING([optimization script])
OPTIMIZE="${OPTIMIZE-./i486-optimize.pl}"
AC_MSG_RESULT([$OPTIMIZE])
AC_ARG_VAR(OPTIMIZE,[optimization script])

if test "$host_alias" = ""; then
  host=i486-linux-glibc
fi

case $host_cpu in
  i?86)
	host_cpu_class=i386;;
  *)
  	host_cpu_class=$host_cpu;;
esac

AC_SUBST(host_cpu_class)

HOST_NATIVE="native/i386"
AC_SUBST(HOST_NATIVE)

AC_ARG_ENABLE([native],
	      AC_HELP_STRING([--enable-native],
			     [native or nonnative (default is native)]),
	      [var_native=$enable_native], [var_native=no] )
if test "$var_native" = "yes"; then
  NO_NATIVE=TRUE
else
  NO_NATIVE=FALSE
fi
AC_SUBST(NO_NATIVE)

AC_ARG_ENABLE([debug],
	      AC_HELP_STRING([--enable-debug],
			     [use debug -g (default is yes)]),
	      [var_debug=$enable_debug] ,[var_debug=yes] )
if test "$var_debug" = "no" ; then
  DEBUGFLAG=""
else
  DEBUGFLAG="-g"
fi
AC_SUBST(DEBUGFLAG)

#AC_MSG_NOTICE([])

AC_ARG_WITH(ccrversion,
[  --with-ccrversion=<ccr8>
   Specify ccr version],
[ccrversion=$withval],
[ccrversion]="")

case "$ccrversion" in
  ccr8)
	CCRFLAG=ccr8 ;;
  *)
	CCRFLAG="" ;;
esac

# Checks for libraries.
#AC_CHECK_LIB([], [])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h stdlib.h string.h sys/param.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_C_VOLATILE

AC_LTDL_SYMBOL_USCORE
if test x"$ac_cv_sys_symbol_underscore" = xyes ; then
  AC_DEFINE(HAVE_SYMBOL_UNDERSCORE, 1, [Define if C symbols have leading underscores])
fi

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([getpagesize memmove memset strchr strdup strerror strstr])

#if test -n "$configdirs"; then
#  AC_CONFIG_SUBDIRS($configdirs)
#fi

#AC_SUBST(SUBDIRS)


# The following is taken from wine-0.9.25's configure.ac

AC_SUBST(EXECSTACK,"")
saved_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -z execstack"
AC_CACHE_CHECK([for -z execstack], ac_cv_z_execstack,
   AC_TRY_LINK([],[int main(void) {return 0;}],
               ac_cv_z_execstack=yes,ac_cv_z_execstack=no))
if test $ac_cv_z_execstack
then
  EXECSTACK="-z execstack"
fi
LDFLAGS="$saved_LDFLAGS"

AM_CONDITIONAL([M68K_HOSTCPU], [test "$host_alias" = "m68k-next-ns3"])

AC_CONFIG_FILES([Makefile
                  syngen/Makefile
                  test/Makefile
                  profile/Makefile
                  runtime/Makefile
                  runtime/native/i386/Makefile
		  include/syn68k_private.h])
dnl                  runtime/native/null/Makefile

AC_OUTPUT
