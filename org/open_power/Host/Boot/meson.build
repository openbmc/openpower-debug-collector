boot_error_hpp = custom_target(
    'error.hpp',
    capture: true,
    command: [
        sdbusplusplus,
        '-r', meson.project_source_root(),
        'error',
        'exception-header',
        'org.open_power.Host.Boot',
    ],
    output: 'error.hpp',
)

boot_error_cpp = custom_target(
    'error.cpp',
    capture: true,
    command: [
        sdbusplusplus,
        '-r', meson.project_source_root(),
        'error',
        'exception-cpp',
        'org.open_power.Host.Boot',
    ],
    output: 'error.cpp',
)
