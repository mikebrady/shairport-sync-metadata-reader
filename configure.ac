#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([shairport-sync-metadata-reader], [1.1], [4265913+mikebrady@users.noreply.github.com])
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
AC_ARG_WITH(plist-pretty-printing, [AS_HELP_STRING([--with-plist-pretty-printing],[build with support for pretty-printing Binary Plists as XML])])
if test "x$with_plist_pretty_printing" = "xyes" ; then
LIBPLIST_PACKAGE=libplist
PKG_CHECK_EXISTS(libplist-2.0, LIBPLIST_PACKAGE=libplist-2.0)
PKG_CHECK_MODULES(
  [libplist], [${LIBPLIST_PACKAGE} >= 2.3.0],
  [CFLAGS="${libplist_CFLAGS} ${CFLAGS}" LIBS="${libplist_LIBS} ${LIBS}" AC_DEFINE([HAVE_LIBPLIST_GE_2_3_0], 1, [libplist >= 2.3.0])],
  [PKG_CHECK_MODULES(
    [libplist], [${LIBPLIST_PACKAGE} >= 2.0.0],
    [CFLAGS="${libplist_CFLAGS} ${CFLAGS}" LIBS="${libplist_LIBS} ${LIBS}"],
    [AC_MSG_ERROR(libplist 2.0.0 or later is required to support plist pretty-printing -- search for pkg libplist-dev on Debian or libplist-2.2.0 or later on FreeBSD!)]
  )]
)
AC_DEFINE([WITH_PLIST_PRETTY_PRINTING], 1, [support pretty-printing Binary Plists as XML.])
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
