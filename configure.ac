#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([shredos_hexviewer], [1.1], [shredos.eraser@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([shredos_hexviewer.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries (checking for ncursesw first, falling back to ncurses)
PKG_CHECK_MODULES([NCURSES], [ncursesw], [], [
    PKG_CHECK_MODULES([NCURSES], [ncurses], [], [
        AC_MSG_ERROR([ncurses development library not found. Please install libncurses-dev or ncurses-devel.])
    ])
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
