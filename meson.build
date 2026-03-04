project(
    'openpower-debug-collector',
    'cpp',
    meson_version: '>=1.1.1',
    default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++23'],
    license: 'Apache-2.0',
    version: '1.0.0',
)

cxx = meson.get_compiler('cpp')

sdbusplus_dep = dependency('sdbusplus')

if cxx.has_header('CLI/CLI.hpp')
    CLI11_dep = declare_dependency()
else
    CLI11_dep = dependency('CLI11')
endif

phosphorlogging = dependency(
    'phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

realpath_prog = find_program('realpath')

conf_data = configuration_data()
extra_deps = []
watchdog_lib = []

# Build PHAL abstraction library FIRST (shared by dump and watchdog)
subdir('phal')

if get_option('hostboot-dump-collection').allowed()
    conf_data.set('WATCHDOG_DUMP_COLLECTION', true)
    extra_deps += [phal_abstraction_dep]  # Use shared PHAL abstraction
    subdir('watchdog')
else
    conf_data.set('WATCHDOG_DUMP_COLLECTION', false)
    watchdog_lib = []
    extra_deps = []
endif

configure_file(output: 'config.h', configuration: conf_data)

if get_option('dump-collection').allowed()
    subdir('dump')
endif

deps = [CLI11_dep, sdbusplus_dep, phosphorlogging, extra_deps]

phal_backend = get_option('phal_backend')
if phal_backend == 'legacy'
    executable(
        'watchdog_timeout',
        'watchdog_timeout.cpp',
        dependencies: deps,
        link_with: watchdog_lib,
        implicit_include_directories: true,
        install: true,
    )
endif

executable(
    'checkstop_app',
    'checkstop_app.cpp',
    dependencies: deps,
    implicit_include_directories: true,
    install: true,
)
