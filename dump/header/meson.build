# SPDX-License-Identifier: Apache-2.0

dump_header_inc = include_directories('.')
project_root_inc = include_directories('../..')
dump_header_core = static_library(
    'dump-header-core',
    'dump_header.cpp',
    'packager.cpp',
    include_directories: dump_header_inc,
)
dump_header_core_dep = declare_dependency(
    include_directories: dump_header_inc,
    link_with: dump_header_core,
)

if dump_collection_enabled or get_option('tests').allowed()
    executable(
        'op-dump-packager',
        'main.cpp',
        'metadata.cpp',
        dependencies: [CLI11_dep, sdbusplus_dep, dump_header_core_dep],
        include_directories: project_root_inc,
        install: dump_collection_enabled,
    )
endif

if get_option('tests').allowed()
    dump_header_test = executable(
        'dump-header-test',
        'test/header_test.cpp',
        dependencies: dump_header_core_dep,
        include_directories: dump_header_inc,
    )
    test('dump-header', dump_header_test)
endif
