#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([shairport-sync-metadata-reader], [1.2], [4265913+mikebrady@users.noreply.github.com])
: ${CFLAGS="-O3"}
: ${CXXFLAGS="-O3"}
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([shairport-sync-metadata-reader.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.

# Look for plist decoding option
AC_ARG_WITH(plist-pretty-printing, [AS_HELP_STRING([--with-plist-pretty-printing],[build with support for pretty-printing Binary Plists])])
if test "x$with_plist_pretty_printing" = "xyes" ; then
AC_DEFINE([WITH_PLIST_PRETTY_PRINTING], 1, [support pretty-printing Binary Plists.])
fi

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset select])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
