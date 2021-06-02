# Generated file; do not modify.
subdir('Boot')
generated_others += custom_target(
    'org/open_power/Host/Boot__markdown'.underscorify(),
    input: [ meson.project_source_root() / 'org/open_power/Host/Boot.errors.yaml',  ],
    output: [ 'Boot.md' ],
    command: [
        sdbuspp_gen_meson_prog, '--command', 'markdown',
        '--output', meson.current_build_dir(),
        '--tool', sdbusplusplus_prog,
        '--directory', meson.project_source_root(),
        'org/open_power/Host/Boot',
    ],
    build_by_default: true,
)

