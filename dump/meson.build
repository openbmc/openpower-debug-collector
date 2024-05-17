# SPDX-License-Identifier: Apache-2.0

cxx = meson.get_compiler('cpp')

sdeventplus_dep = dependency('sdeventplus')

collect_deps = [
   CLI11_dep,
   phosphorlogging,
   cxx.find_library('pdbg'),
   cxx.find_library('libdt-api'),
   cxx.find_library('phal'),
]

monitor_deps = [
    sdbusplus_dep,
    sdeventplus_dep,
    phosphorlogging,
]

# source files

collect_src = files(
    'sbe_dump_collector.cpp',
    'dump_collect_main.cpp',
    'dump_utils.cpp',
    'create_pel.cpp',
    'dump_utils.cpp',
    'sbe_type.cpp',
)

monitor_src = files(
    'dump_monitor.cpp',
    'dump_monitor_main.cpp',
    'dump_utils.cpp',
)

executable('dump-collect',
    collect_src,
    dependencies: collect_deps,
    implicit_include_directories: true,
    install: true
)

executable('openpower-dump-monitor',
    monitor_src,
    dependencies: monitor_deps,
    implicit_include_directories: true,
    install: true
)

bindir = get_option('bindir')
dreport_include_dir = join_paths(get_option('datadir'), 'dreport.d/include.d')

scripts_to_install = []
include_scripts = []

subdir('tools')
# Install collected files if any
if scripts_to_install.length() > 0
    install_data(scripts_to_install,
        install_dir: get_option('bindir'),
        install_mode: 'rwxr-xr-x'
    )
endif

# Install collected include scripts if any
if include_scripts.length() > 0
    install_data(include_scripts,
        install_dir: dreport_include_dir,
        install_mode: 'rwxr-xr-x'
    )
endif

subdir('dist')


