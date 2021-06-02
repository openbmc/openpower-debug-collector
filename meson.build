project(
    'openpower-debug-collector',
    'cpp',
    meson_version: '>= 0.57.0',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++20'
    ],
    license: 'Apache-2.0',
    version: '1.0.0',
)

sdbusplus = dependency('sdbusplus', required : true)
systemd = dependency('systemd', required : true)
phosphorlogging = dependency('phosphor-logging', required : true)

sdbusplusplus = find_program('sdbus++', required : true)

if get_option('gen_errors')
    subdir('phosphor-logging')
endif

subdir('org/open_power/Host/Boot')

executable('watchdog_timeout',
           'watchdog_timeout.cpp',
           boot_error_hpp,
           boot_error_cpp,
           dependencies: [
               sdbusplus,
               systemd,
               phosphorlogging
           ],
           implicit_include_directories: true,
           install: true
)

executable('checkstop_app',
           'checkstop_app.cpp',
           boot_error_hpp,
           boot_error_cpp,
           dependencies: [
               sdbusplus,
               systemd,
               phosphorlogging
           ],
           implicit_include_directories: true,
           install: true
)
