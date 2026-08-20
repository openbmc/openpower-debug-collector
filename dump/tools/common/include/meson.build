include_scripts += meson.current_source_dir() / 'gendumpinfo'

opfunctions_conf = configuration_data()
opfunctions_conf.set('PHAL_BACKEND', phal_backend)
include_scripts += configure_file(
    configuration: opfunctions_conf,
    input: 'opfunctions',
    output: 'opfunctions',
)
