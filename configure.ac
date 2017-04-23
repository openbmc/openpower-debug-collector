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

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_14([noext])
AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

# Python
AM_PATH_PYTHON([2.7], [AC_SUBST([PYTHON], [echo "$PYTHON"])], [AC_MSG_ERROR([Could not find python-2.7 installed...python-2.7 is required])])

LT_INIT

# Checks for libraries.
AX_PKG_CHECK_MODULES([SYSTEMD], [], [libsystemd >= 221], [],
[AC_MSG_ERROR(["systemd required and not found."])])

# Checks for modules
PKG_CHECK_MODULES([SDBUSPLUS], [sdbusplus],, [AC_MSG_ERROR([Could not find sdbusplus...openbmc/sdbusplus package required])])
PKG_CHECK_MODULES([PHOSPHOR_LOGGING], [phosphor-logging],, [AC_MSG_ERROR([Could not find phosphor-logging...openbmc/phosphor-logging package required])])

# Check for header files.
AC_CHECK_HEADER(phosphor-logging/log.hpp, ,[AC_MSG_ERROR([Could not find phosphor-logging/log.hpp...openbmc/phosphor-logging package required])])
AC_CHECK_HEADER(phosphor-logging/elog.hpp, ,[AC_MSG_ERROR([Could not find phosphor-logging/elog.hpp...openbmc/phosphor-logging package required])])

# Handling configuration files
AC_ARG_VAR(YAML_DIR, [The path to the yaml error files.])
AS_IF([test "x$YAML_DIR" == "x"], \
    [YAML_DIR="${srcdir}"])

# Compile error metadata handlers if we're asked to do so.
AC_ARG_ENABLE([metadata-processing],
    AS_HELP_STRING([--enable-metadata-processing], [Compile metadata handlers]),
        [AX_APPEND_COMPILE_FLAGS([-DPROCESS_META], [CXXFLAGS])]
)

# Create configured output
AC_CONFIG_FILES([Makefile ])
AC_OUTPUT
