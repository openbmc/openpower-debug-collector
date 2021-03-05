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

AC_ARG_ENABLE([enable-dump-collection],
        [
AS_HELP_STRING([--enable-dump-collection],
                [To specify whether dump offload is enabled])])
AM_CONDITIONAL([ENABLE_DUMP_COLLECTION], [test "x$enable_dump_collection" == "xyes"])
AS_IF([test "x$enable_dump_collection" = "xyes"], [
    AC_CHECK_LIB([pdbg], [sbe_dump])

    PKG_CHECK_MODULES([LIBDT_API], [libdt-api])
    LIBS="$LIBS $LIBDT_API_LIBS"
    CFLAGS="$CFLAGS $LIBDT_API_CFLAGS"
])

PKG_CHECK_MODULES([SDEVENTPLUS], [sdeventplus])
# Generate elog-errors.hpp if gen_errors optional feature is enabled
AC_ARG_ENABLE([gen_errors],
    AS_HELP_STRING([--enable-gen_errors], [Enable elog-errors.hpp generation ]),
    [],[gen_errors=yes])
AM_CONDITIONAL([GEN_ERRORS], [test "x$enable_gen_errors" != "xno"])

# Check for compiler, packages only when install_error_yaml optional feature
# is disabled
AC_ARG_ENABLE([install_error_yaml],
    AS_HELP_STRING([--enable-install_error_yaml], [Enable installing error yaml file]),
    [], [install_error_yaml=no])
AM_CONDITIONAL([INSTALL_ERROR_YAML], [test "x$enable_install_error_yaml" = "xyes"])
AS_IF([test "x$enable_install_error_yaml" != "xyes"], [
    # Checks for typedefs, structures, and compiler characteristics.
    AX_CXX_COMPILE_STDCXX_17([noext])
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
