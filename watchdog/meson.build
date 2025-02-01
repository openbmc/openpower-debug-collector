# Source files
watchdog_src = files(
    'ffdc_file.cpp',
    'utils.cpp',
    'watchdog_common.cpp',
    'watchdog_dbus.cpp',
    'watchdog_handler.cpp',
    'watchdog_logging.cpp',
    'watchdog_main.cpp',
)

# Library dependencies
watchdog_deps = [sdbusplus_dep]

# Create static library
watchdog_lib = static_library(
    'watchdog_lib',
    watchdog_src,
    dependencies: watchdog_deps,
    install: false,
)
