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
    CLI11_dep = dependency(
        'CLI11',
        include_type: 'system',
        default_options: ['tests=disabled'],
    )
endif

phosphorlogging = dependency(
    'phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

realpath_prog = find_program('realpath')

phal_backend = get_option('phal_backend')
dump_collection_enabled = get_option('dump-collection').allowed()
hostboot_collection_enabled = get_option(
    'hostboot-dump-collection',
).allowed()

if dump_collection_enabled and phal_backend == 'none'
    error(
        'dump-collection requires -Dphal_backend=legacy or next',
    )
endif

if hostboot_collection_enabled and phal_backend != 'legacy'
    error(
        'hostboot-dump-collection is supported only by the P10 legacy backend',
    )
endif

conf_data = configuration_data()
conf_data.set_quoted('PHAL_BACKEND', phal_backend)
extra_deps = []
watchdog_lib = []

if dump_collection_enabled or hostboot_collection_enabled
    subdir('phal')
endif

if hostboot_collection_enabled
    conf_data.set('WATCHDOG_DUMP_COLLECTION', true)
    extra_deps += [phal_abstraction_dep]
    subdir('watchdog')
else
    conf_data.set('WATCHDOG_DUMP_COLLECTION', false)
endif

configure_file(output: 'config.h', configuration: conf_data)

if dump_collection_enabled or get_option('tests').allowed()
    subdir('dump/header')
endif

if dump_collection_enabled
    subdir('dump')
endif

deps = [CLI11_dep, sdbusplus_dep, phosphorlogging] + extra_deps

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
