project(
    'openpower-debug-collector',
    'cpp',
    meson_version: '>= 0.57.0',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++20'
    ],
    license: 'Apache-2.0',
    version: '1.0.0',
)

systemd = dependency('systemd', required : true)

sdbusplus = dependency(
    'sdbusplus',
    fallback: [
        'sdbusplus',
        'sdbusplus_dep'
    ]
)
sdbusplusplus_prog = find_program('sdbus++', required : true)
sdbuspp_gen_meson_prog = find_program('sdbus++-gen-meson', required : true)

phosphorlogging = dependency(
    'phosphor-logging',
    fallback: [
        'phosphor-logging',
        'phosphor_logging_dep'
    ]
)

fmt_dep = dependency('fmt', required: false)
if not fmt_dep.found()
  fmt_proj = import('cmake').subproject(
    'fmt',
    cmake_options: [
      '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
      '-DMASTER_PROJECT=OFF'
    ],
    required: false)
  assert(fmt_proj.found(), 'fmtlib is required')
  fmt_dep = fmt_proj.dependency('fmt')
endif

cxx = meson.get_compiler('cpp')

deps = [
    systemd,
    sdbusplus,
    phosphorlogging,
    fmt_dep,
    cxx.find_library('pdbg'),
    cxx.find_library('libdt-api'),
    cxx.find_library('phal')
]

realpath_prog = find_program('realpath')

selected_subdirs = []
selected_subdirs += 'org/open_power'

generated_root = meson.current_build_dir() / 'gen'
generated_others = []
generated_sources = []

# Source the generated meson files
subdir('gen')
foreach d : selected_subdirs
    subdir('gen' / d)
endforeach

# Parse through the list from sdbus++-gendir and put into sets.
generated_headers = []
generated_cpp = []
generated_others_files = []

foreach g : generated_sources generated_others
    foreach f : g.to_list()
        rel_path = run_command(
            realpath_prog,
            '--relative-to', generated_root,
            f.full_path(),
        ).stdout().strip().split('\n')[-1]

        if rel_path.endswith('.hpp')
            generated_headers += rel_path
        elif rel_path.endswith('.cpp')
            generated_cpp += rel_path
        else
            generated_others_files += rel_path
        endif
    endforeach
endforeach

conf_data = configuration_data()
if get_option('hostboot-dump-collection').enabled()
    conf_data.set('HOSTBOOT_SBE_DUMP_COLLECTION', true)
    subdir('watchdog')
else
    conf_data.set('HOSTBOOT_SBE_DUMP_COLLECTION', false)
    watchdog_lib = []
endif

executable('watchdog_timeout',
    'watchdog_timeout.cpp',
    configure_file(output: 'config.h', configuration: conf_data),
    generated_sources,
    dependencies: deps,
    link_with: watchdog_lib,
    include_directories: include_directories('gen'),
    implicit_include_directories: true,
    install: true
)

executable('checkstop_app',
    'checkstop_app.cpp',
    generated_sources,
    dependencies: deps,
    include_directories: include_directories('gen'),
    implicit_include_directories: true,
    install: true
)
