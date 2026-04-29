# SPDX-License-Identifier: Apache-2.0

cxx = meson.get_compiler('cpp')

# Use shared PHAL abstraction library
# sdbusplus needed for dump_utils.cpp (D-Bus calls)
# Note: phal_backend variable is set in parent meson.build
if phal_backend != 'none'
    collect_deps = [
        CLI11_dep,
        sdbusplus_dep,
        phosphorlogging,
        phal_abstraction_dep,
    ]
else
    collect_deps = [
        CLI11_dep,
        sdbusplus_dep,
        phosphorlogging,
    ]
endif
monitor_deps = [sdbusplus_dep, phosphorlogging]

# source files

collect_src = files(
    'dump_collect_main.cpp',
    'dump_utils.cpp',
    'sbe_dump_collector.cpp',
    'sbe_type.cpp',
)

# Only include create_pel for legacy PHAL backend
if phal_backend == 'legacy'
    collect_src += files('create_pel.cpp')
    # create_pel.cpp needs legacy PHAL libraries (libekb, phal)
    collect_deps += [
        cxx.find_library('phal'),
        cxx.find_library('ekb'),
    ]
endif

monitor_src = files(
    'dump_monitor.cpp',
    'dump_monitor_main.cpp',
    'dump_utils.cpp',
)

phal_inc = include_directories('../phal')

executable(
    'dump-collect',
    collect_src,
    dependencies: collect_deps,
    include_directories: phal_inc,
    implicit_include_directories: true,
    install: true,
)

executable(
    'openpower-dump-monitor',
    monitor_src,
    dependencies: monitor_deps,
    implicit_include_directories: true,
    install: true,
)

bindir = get_option('bindir')
dreport_include_dir = join_paths(get_option('datadir'), 'dreport.d/include.d')
dreport_plugins_dir = join_paths(get_option('datadir'), 'dreport.d/plugins.d')

scripts_to_install = []
plugins_to_install = []
include_scripts = []

subdir('tools')
# Install collected files if any
if scripts_to_install.length() > 0
    install_data(
        scripts_to_install,
        install_dir: get_option('bindir'),
        install_mode: 'rwxr-xr-x',
    )
endif

# Install collected plugins if any
if plugins_to_install.length() > 0
    install_data(
        plugins_to_install,
        install_dir: dreport_plugins_dir,
        install_mode: 'rwxr-xr-x',
    )
endif

# Install collected include scripts if any
if include_scripts.length() > 0
    install_data(
        include_scripts,
        install_dir: dreport_include_dir,
        install_mode: 'rwxr-xr-x',
    )
endif

subdir('dist')
