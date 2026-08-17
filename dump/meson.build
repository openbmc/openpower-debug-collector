# SPDX-License-Identifier: Apache-2.0

cxx = meson.get_compiler('cpp')

sdeventplus_dep = dependency('sdeventplus')

if phal_backend == 'legacy'
    collect_deps = [
        CLI11_dep,
        sdbusplus_dep,
        phosphorlogging,
        phal_abstraction_dep,
    ]
    monitor_deps = [sdbusplus_dep, sdeventplus_dep, phosphorlogging]

    collect_src = files(
        'dump_collect_main.cpp',
        'dump_utils.cpp',
        'sbe_dump_collector.cpp',
        'sbe_type.cpp',
    )

    # The P10 backend retains its libphal FFDC-to-PEL implementation.
    collect_src += files('create_pel.cpp')
    collect_deps += cxx.find_library('ekb')

    monitor_src = files(
        'dump_action_monitor.cpp',
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
endif

bindir = get_option('bindir')
dreport_include_dir = join_paths(get_option('datadir'), 'dreport.d/include.d')
dreport_plugins_dir = join_paths(get_option('datadir'), 'dreport.d/plugins.d')

scripts_to_install = []
plugins_to_install = []
include_scripts = []

subdir('tools')

if scripts_to_install.length() > 0
    install_data(
        scripts_to_install,
        install_dir: get_option('bindir'),
        install_mode: 'rwxr-xr-x',
    )
endif

if plugins_to_install.length() > 0
    install_data(
        plugins_to_install,
        install_dir: dreport_plugins_dir,
        install_mode: 'rwxr-xr-x',
    )
endif

if include_scripts.length() > 0
    install_data(
        include_scripts,
        install_dir: dreport_include_dir,
        install_mode: 'rwxr-xr-x',
    )
endif

if phal_backend == 'legacy'
    subdir('dist')
endif
