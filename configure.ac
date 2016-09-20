#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([shairport-sync-metadata-reader], [1.0.1], [mikebrady@eircom.net])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([shairport-sync-metadata-reader.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.

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
