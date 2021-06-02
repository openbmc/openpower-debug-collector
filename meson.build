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

sdbusplus = dependency('sdbusplus', required : false)
if sdbusplus.found()
    sdbusplusplus_prog = find_program('sdbus++', required : true)
    sdbuspp_gen_meson_prog = find_program('sdbus++-gen-meson', required : true)
else
    sdbusplus_proj = subproject('sdbusplus', required: true)
    sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
    sdbusplusplus_prog = sdbusplus_proj.get_variable('sdbusplusplus_prog')
    sdbuspp_gen_meson_prog = sdbusplus_proj.get_variable(
        'sdbuspp_gen_meson_prog'
    )
endif

phosphorlogging = dependency(
    'phosphor-logging',
    fallback: [
        'phosphor-logging',
        'phosphor_logging_dep'
    ]
)

realpath_prog = find_program('realpath')

if get_option('gen_errors')
    # TODO: Build elog-errors.log when elog-gen.py available
endif

# subdir('org/open_power/Host/Boot')
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

executable('watchdog_timeout',
           'watchdog_timeout.cpp',
            generated_sources,
           dependencies: [
               sdbusplus,
               systemd,
               phosphorlogging
           ],
           include_directories: include_directories('gen'),
           implicit_include_directories: true,
           install: true
)

executable('checkstop_app',
           'checkstop_app.cpp',
            generated_sources,
           dependencies: [
               sdbusplus,
               systemd,
               phosphorlogging
           ],
           include_directories: include_directories('gen'),
           implicit_include_directories: true,
           install: true
)
