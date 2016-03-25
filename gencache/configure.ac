AC_INIT([gencache],[1.0],[https://github.com/EasyRPG/Tools/issues],[gencache],[https://easy-rpg.org/])

AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AM_SILENT_RULES([yes])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_FILES([Makefile])
AC_LANG([C++])
AC_PROG_CXX

AX_CXX_COMPILE_STDCXX(11, noext)
PKG_CHECK_MODULES([ICU], [icu-i18n])

AC_OUTPUT
