dnl
dnl $Id$
dnl 
dnl Additional autoconf macros for TEA
dnl 
dnl

dnl _TEAX_CHECK_MSVC --
dnl
dnl Defines the shell variable "USING_MSVC" as "yes" or "no".
dnl Several tests on Windows work differently depending on
dnl whether we're using MSVC or GCC.  Current heuristic:
dnl if it's Windows, and CC is not gcc, we're using MSVC.
dnl
dnl This macro doesn't need to be called from configure.ac;
dnl other macros that need it will AC_REQUIRE it.
dnl
AC_DEFUN([_TEAX_CHECK_MSVC],[
#@ _TEAX_CHECK_MSVC
AC_REQUIRE([AC_PROG_CC])
if test "${TEA_PLATFORM}" = "windows" -a "$GCC" != "yes"; then
    USING_MSVC="yes"
else
    USING_MSVC="no"
fi;
])

dnl TEAX_CONFIG_LDFLAGS --
dnl 
dnl Sets the following additional variables used when building libraries:
dnl
dnl	SHLIB_LD_OUT
dnl	    Either "-o " or "/out:" depending on whether SHLIB_LD
dnl	    is a Unix-style linker (ld) or MS-style (link)
dnl
dnl	STLIB_LD_OUT --
dnl 	    Either " " or "/out:" depending on whether STLIB_LD
dnl 	    is Unix-style "ar" or MS-style (lib)
dnl
dnl	SHLIB_SUFFIX --
dnl	    Suffix for shared libraries.  This is actually computed by
dnl 	    TEA_CONFIG_CFLAGS, but it doesn't AC_SUBST() it.
dnl
dnl 	STLIB_SUFFIX --
dnl	    Suffix for static libraries (.a, .lib, ...)
dnl
dnl	LIB_PREFIX --
dnl	    Prefix for library names; either "lib" or empty.
dnl
AC_DEFUN([TEAX_CONFIG_LDFLAGS],[
#@ TEAX_CONFIG_LDFLAGS
    AC_REQUIRE([_TEAX_CHECK_MSVC])
    if test "${USING_MSVC}" = "yes"; then
	SHLIB_LD_OUT="/out:"
	STLIB_LD_OUT="/out:"
	STLIB_SUFFIX=".lib"
	LIB_PREFIX=""
    else
	SHLIB_LD_OUT="-o "
	STLIB_LD_OUT=" "
	STLIB_SUFFIX=".a"
	LIB_PREFIX="lib"
    fi
    AC_SUBST(SHLIB_LD_OUT)
    AC_SUBST(STLIB_LD_OUT)
    AC_SUBST(SHLIB_SUFFIX)
    AC_SUBST(STLIB_SUFFIX)
    AC_SUBST(LIB_PREFIX)
])

dnl TEAX_EXPAND_CFLAGS --
dnl	Computes final value of CFLAGS macro.
dnl
dnl	Uses the same logic as TEA_MAKE_LIB, except that
dnl	${CFLAGS_DEFAULT}, ${CFLAGS_WARNING}, and ${SHLIB_CFLAGS}
dnl 	are expanded at configure-time instead of at make-time.
dnl 
AC_DEFUN([TEAX_EXPAND_CFLAGS],[
#@ TEAX_EXPAND_CFLAGS
    AC_REQUIRE([TEA_ENABLE_SYMBOLS])
    CFLAGS="${CFLAGS} ${CFLAGS_DEFAULT} ${CFLAGS_WARNING}"
    if test "${SHARED_BUILD}" = "1" ; then
    	CFLAGS="${CFLAGS} ${SHLIB_CFLAGS}"
    fi
])

dnl TEAX_FIX_LIB_SPECS --
dnl	TCL_STUB_LIB_SPEC is supposed to contain the linker flags
dnl 	for picking up the Tcl stub library; however, the settings
dnl 	in tclConfig.sh only work on Unix and with GCC on Windows.
dnl 	TEAX_FIX_LIB_SPECS adjusts TCL_STUB_LIB_SPEC et. al. so
dnl	they work with MSVC as well. 
dnl
dnl	(TEA_MAKE_LIB works around this in a different way.)
dnl
AC_DEFUN([TEAX_FIX_LIB_SPECS],[
#@TEAX_FIX_LIB_SPECS
AC_REQUIRE([_TEAX_CHECK_MSVC])
if test "${USING_MSVC}" = "yes"; then
    TCL_STUB_LIB_SPEC="$TCL_STUB_LIB_PATH"
    TK_STUB_LIB_SPEC="$TK_STUB_LIB_PATH"
    # tclConfig.sh doesn't define TCL_LIB_PATH, but if it did,
    # it would be as follows:
    eval TCL_LIB_SPEC="${TCL_EXEC_PREFIX}/lib/$TCL_LIB_FILE"
    # Same for TK_LIB_PATH:
    eval TK_LIB_SPEC="${TK_EXEC_PREFIX}/lib/$TK_LIB_FILE"
fi
])

