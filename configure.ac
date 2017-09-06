# Initialization
AC_PREREQ([2.69])
AC_INIT([openpower-debug-collector], [1.0],[https://github.com/openbmc/openpower-debug-collector/issues])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AM_PATH_PYTHON([2.7], [AC_SUBST([PYTHON], [echo "$PYTHON"])])
AC_SUBST([PYTHONDIR], ${pythondir})
AS_IF([test "$PYTHON" != :], [have_python=yes], [have_python=no])

AC_ARG_ENABLE([eloggen],
    AS_HELP_STRING([--disable-eloggen], [Disable eloggen]))

AS_IF([test "x$enable_eloggen" != "xno"],
    [AS_IF([test "x$have_python" = "xno"], [AC_MSG_ERROR([eloggen requires python])])])
AM_CONDITIONAL([WANT_ELOGGEN], [test "x$enable_eloggen" != "xno"])

AC_ARG_ENABLE([debug_collector_app],
    AS_HELP_STRING([--disable-debug_collector_app], [Disable debug_collector_app]))

AM_CONDITIONAL([WANT_DEBUG_COLLECTOR_APP], [test "x$enable_debug_collector_app" != "xno"])
AS_IF([test "x$enable_debug_collector_app" != "xno"], [
    # Checks for typedefs, structures, and compiler characteristics.
    AX_CXX_COMPILE_STDCXX_14([noext])
    AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

    # Check for sdbus++
    AC_PATH_PROG([SDBUSPLUSPLUS], [sdbus++])
    AS_IF([test "x$SDBUSPLUSPLUS" == "x"],
        AC_MSG_ERROR(["Requires sdbus++"]))

    LT_INIT

    # Checks for libraries.
    AX_PKG_CHECK_MODULES([SYSTEMD], [], [libsystemd >= 221], [],
        [AC_MSG_ERROR(["systemd required and not found."])])

    # Checks for modules
    PKG_CHECK_MODULES([PHOSPHOR_LOGGING], [phosphor-logging],,
        [AC_MSG_ERROR([Could not find phosphor-logging...openbmc/phosphor-logging package required])])
])
# Create configured output
AC_CONFIG_FILES([Makefile tools/Makefile tools/setup.py])
AC_CONFIG_FILES([openpower-debug-collector.pc])
AC_OUTPUT
