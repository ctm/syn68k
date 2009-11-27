AC_INIT(syn68k, 1.0, ctm@ardi.com)
LT_INIT
AM_INIT_AUTOMAKE

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([syngen/main.c])
AC_CONFIG_HEADERS([include/config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_AS

AC_CHECK_PROG([PERL], [perl], [perl])
AC_ARG_VAR([PERL], [perl for preening assembly])

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

# See lengthy explanation in runtime/Makefile.am for dce_fixup explanation

if test x$host_cpu = xx86_64; then
  dce_fixup=-fno-dce
fi

AC_SUBST(dce_fixup)

AC_MSG_CHECKING([Syn68k C flags (TODO: FIXME)])
SYN68K_CFLAGS="${SYN68K_CFLAGS--O2 -fomit-frame-pointer}"
AC_MSG_RESULT([$SYN68K_CFLAGS])
AC_ARG_VAR(SYN68K_CFLAGS,[Syn68k C flags (TODO: FIXME)])

AC_ARG_ENABLE([native],
	      AC_HELP_STRING([--enable-native],
			  [recompile into x86 code (default enabled for x86)]),
	      [], [if test x$host_cpu_class = xi386; then
                     enable_native=yes
                   else
                     enable_native=no
                   fi])

AC_MSG_CHECKING([native support])
if test x$enable_native = xyes; then
  if test x$host_cpu_class != xi386; then
    AC_MSG_ERROR([Can't build native on non-x86])
  fi
  HOST_NATIVE="native/i386"
  NONNATIVE=""
else
  HOST_NATIVE="native/null"
  NONNATIVE="#define NONNATIVE"
fi
AC_MSG_RESULT([$enable_native])

AC_SUBST(NONNATIVE)
AC_SUBST(HOST_NATIVE)

AC_ARG_ENABLE([assembly-cleanup],
              AC_HELP_STRING([--enable-assembly-cleanup],
                             [remove unneeded assembler instructions (default enabled for x86)]),
              [], [if test x$host_cpu_class = xi386; then
                     enable_assembly_cleanup=yes
                   else
                     enable_assembly_cleanup=no
                   fi])

AC_MSG_CHECKING([assembler cleanup])
if test x$enable_assembly_cleanup = xyes; then
  assembly_cleanup_script=./i486-cleanup.pl
else
  assembly_cleanup_script=
fi
AC_MSG_RESULT([$enable_assembly_cleanup])
AC_SUBST(assembly_cleanup_script)


# TODO: finish this
AC_ARG_ENABLE([assembly-optimization],
              AC_HELP_STRING([--enable-assembly-optimization],
                             [reorder assembly instructions (x86 only; default is no]),
              [], [enable_assembly_optimization=no])

AC_MSG_CHECKING([assembly optimization])
if test x$enable_assembly_optimization = xyes; then
  assembly_optimization_script=./i486-optimize.pl
else
  assembly_optimization_script=
fi
AC_MSG_RESULT($enable_assembly_optimization)
AC_SUBST(assembly_optimization_script)

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

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h stdlib.h string.h sys/param.h unistd.h stdint.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_C_VOLATILE

AC_LTDL_SYMBOL_USCORE
if test x"$ac_cv_sys_symbol_underscore" = xyes ; then
  AC_DEFINE(HAVE_SYMBOL_UNDERSCORE, 1,
           [Define if C symbols have leading underscores])
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

AC_CHECK_SIZEOF([char *])

AC_TYPE_INT8_T
AC_TYPE_UINT8_T

AC_TYPE_INT16_T
AC_TYPE_UINT16_T

AC_TYPE_INT32_T
AC_TYPE_UINT32_T

AC_TYPE_INT64_T
AC_TYPE_UINT64_T

# The following is taken from wine-0.9.25's configure.ac

AC_SUBST(EXECSTACK,"")
saved_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -z execstack"
AC_CACHE_CHECK([for -z execstack], ac_cv_z_execstack,
   AC_TRY_LINK([],[int main(void) {return 0;}],
               ac_cv_z_execstack=yes,ac_cv_z_execstack=no))
if test x$ac_cv_z_execstack = xyes; then
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

AC_OUTPUT
