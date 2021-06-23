# Source files
hbdump_src = files(
    'hbdump_dbus.cpp',
    'hbdump_logging.cpp',
    'hbdump_handler.cpp',
    'hbdump_common.cpp',
    'hbdump_main.cpp',
)

# Library dependencies
hbdump_deps = [
    sdbusplus
]

# Create static library
hbdump_lib = static_library(
    'hbdump_lib',
    hbdump_src,
    dependencies: hbdump_deps,
    install: false,
)
