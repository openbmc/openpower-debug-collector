# SPDX-License-Identifier: Apache-2.0

cxx = meson.get_compiler('cpp')

collect_deps = [
   phosphorlogging,
   cxx.find_library('pdbg'),
   cxx.find_library('libdt-api'),
   cxx.find_library('phal'),
]

# source files

collect_src = files(
    'argument.cpp',
    'dump_collect.cpp',
    'dump_collect_main.cpp',
    'argument.cpp',
    'dump_utils.cpp',
    'create_pel.cpp',
)

executable('dump-collect',
    collect_src,
    dependencies: collect_deps,
    implicit_include_directories: true,
    install: true
)
