# Source files
watchdog_src = files(
    'watchdog_dbus.cpp',
    'watchdog_logging.cpp',
    'watchdog_handler.cpp',
    'watchdog_common.cpp',
    'watchdog_main.cpp',
    'utils.cpp',
    'ffdc_file.cpp',
)

# Library dependencies
watchdog_deps = [
    sdbusplus
]

# Create static library
watchdog_lib = static_library(
    'watchdog_lib',
    watchdog_src,
    dependencies: watchdog_deps,
    install: false,
)
