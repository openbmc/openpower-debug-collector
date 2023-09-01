project(
    'openpower-debug-collector',
    'cpp',
    meson_version: '>=1.1.1',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++23'
    ],
    license: 'Apache-2.0',
    version: '1.0.0',
)

cxx = meson.get_compiler('cpp')

sdbusplus_dep = dependency('sdbusplus')
sdbusplusplus_prog = find_program('sdbus++')
sdbuspp_gen_meson_prog = find_program('sdbus++-gen-meson')
sdbusplusplus_depfiles = files()
if sdbusplus_dep.type_name() == 'internal'
    sdbusplusplus_depfiles = subproject('sdbusplus').get_variable('sdbusplusplus_depfiles')
endif

if cxx.has_header('CLI/CLI.hpp')
    CLI11_dep = declare_dependency()
else
    CLI11_dep = dependency('CLI11')
endif

phosphorlogging = dependency(
    'phosphor-logging',
    fallback: [
        'phosphor-logging',
        'phosphor_logging_dep'
    ]
)

realpath_prog = find_program('realpath')

conf_data = configuration_data()
if get_option('hostboot-dump-collection').enabled()
    conf_data.set('WATCHDOG_DUMP_COLLECTION', true)
    extra_deps = [
        cxx.find_library('pdbg'),
        cxx.find_library('libdt-api'),
        cxx.find_library('phal')
    ]
    subdir('watchdog')
else
    conf_data.set('WATCHDOG_DUMP_COLLECTION', false)
    watchdog_lib = []
    extra_deps = []
endif

configure_file(output: 'config.h', configuration: conf_data)

deps = [
    CLI11_dep,
    sdbusplus_dep,
    phosphorlogging,
    extra_deps
]

executable('watchdog_timeout',
    'watchdog_timeout.cpp',
    dependencies: deps,
    link_with: watchdog_lib,
    include_directories: include_directories('gen'),
    implicit_include_directories: true,
    install: true
)

executable('checkstop_app',
    'checkstop_app.cpp',
    dependencies: deps,
    include_directories: include_directories('gen'),
    implicit_include_directories: true,
    install: true
)
