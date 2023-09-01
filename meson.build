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

systemd = dependency('systemd', required : true)

sdbusplus_dep = dependency('sdbusplus')

phosphorlogging = dependency(
    'phosphor-logging',
    fallback: [
        'phosphor-logging',
        'phosphor_logging_dep'
    ]
)

realpath_prog = find_program('realpath')

cxx = meson.get_compiler('cpp')
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
    systemd,
    sdbusplus_dep,
    phosphorlogging,
    extra_deps
]

executable('watchdog_timeout',
    'watchdog_timeout.cpp',
    dependencies: deps,
    link_with: watchdog_lib,
    implicit_include_directories: true,
    install: true
)

executable('checkstop_app',
    'checkstop_app.cpp',
    dependencies: deps,
    implicit_include_directories: true,
    install: true
)
