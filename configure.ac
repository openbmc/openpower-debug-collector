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

#Generate elog-errors.hpp only during local repository bulld
#Disabled during bitbake build
AC_ARG_ENABLE([elogheader],
    AS_HELP_STRING([--enable-elogheader], [Enable elog error header generation]))
AM_CONDITIONAL([WANT_ELOG_HEADER], [test "x$enable_elogheader" != "xno"])

AC_ARG_ENABLE([debug_collector],
    AS_HELP_STRING([--enable-debug_collector], [Enable debug_collector]))
AM_CONDITIONAL([WANT_DEBUG_COLLECTOR], [test "x$enable_debug_collector" != "xno"])
# Check for programs only for non native compilation
AS_IF([test "x$enable_debug_collector" != "xno"], [
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
AC_CONFIG_FILES([Makefile ])
AC_OUTPUT
