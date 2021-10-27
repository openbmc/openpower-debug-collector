# SPDX-License-Identifier: Apache-2.0

unit_files += {'input':'dump/org.open_power.Dump.Manager.service',
         'output':'org.open_power.Dump.Manager.service'}

cxx = meson.get_compiler('cpp')

sdeventplus_dep = dependency('sdeventplus')

dump_deps = [
    systemd,
    sdbusplus_dep,
    phosphor_dbus_interfaces_dep,
    sdeventplus_dep,
    fmt_dep,
    phosphorlogging,
]

collect_deps = [
   fmt_dep,
   phosphorlogging,
   cxx.find_library('pdbg'),
   cxx.find_library('libdt-api'),
   cxx.find_library('phal'),
]

# Configuration header file(dump_config.h) generation
dump_conf_data = configuration_data()
dump_conf_data.set_quoted('OP_DUMP_OBJPATH', get_option('OP_DUMP_OBJPATH'),
                     description : 'The D-Bus root of openpower-dump-collector')
dump_conf_data.set_quoted('OP_DUMP_BUSNAME', get_option('OP_DUMP_BUSNAME'),
                     description : 'The bus name of openpower-dump-collector')
configure_file(configuration : dump_conf_data,
               output : 'config.h')

# source files
dump_src = files(
    'dump_manager_main.cpp',
    'dump_utils.cpp',
    'dump_manager.cpp',
)

collect_src = files(
    'argument.cpp',
    'dump_utils.cpp',
    'dump_collect.cpp',
    'dump_collect_main.cpp',
)


executable('openpower-dump-manager',
    dump_src,
    dependencies: dump_deps,
    implicit_include_directories: true,
    install: true
)

executable('dump-collect',
    collect_src,
    dependencies: collect_deps,
    implicit_include_directories: true,
    install: true
)
