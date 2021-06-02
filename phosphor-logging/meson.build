copy_prog = find_program('cp', required : true)

custom_target(
    'elog-errors.hpp',
    command: [
        copy_prog,
        meson.project_source_root() / 'phosphor-logging/elog-errors.hpp',
        meson.current_build_dir() / 'elog-errors.hpp',
    ],
    build_by_default: true,
    output: 'elog-errors.hpp',
)

