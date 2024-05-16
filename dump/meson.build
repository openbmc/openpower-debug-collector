# SPDX-License-Identifier: Apache-2.0

cxx = meson.get_compiler('cpp')

collect_deps = [
   CLI11_dep,
   phosphorlogging,
   cxx.find_library('pdbg'),
   cxx.find_library('libdt-api'),
   cxx.find_library('phal'),
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

executable('dump-collect',
    collect_src,
    dependencies: collect_deps,
    implicit_include_directories: true,
    install: true
)

bindir = get_option('bindir')

scripts_to_install = [] 

subdir('tools')
# Install collected files if any
if scripts_to_install.length() > 0
    install_data(scripts_to_install,
        install_dir: get_option('bindir'),
        install_mode: 'rwxr-xr-x'
    )
endif

