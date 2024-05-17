# SPDX-License-Identifier: Apache-2.0

systemd_dep = dependency('systemd', required: true)

systemd_system_unit_dir = systemd_dep.get_variable(
    pkgconfig: 'systemdsystemunitdir'
)
dist_conf_data = configuration_data()
dist_conf_data.set('bindir', get_option('prefix') / get_option('bindir'))

# Service installation
configure_file(
  input: 'openpower-dump-monitor.service',
  output: 'openpower-dump-monitor.service',
  configuration: dist_conf_data,
  install: true,
  install_dir: systemd_system_unit_dir
)

# Symlinks for services
systemd_alias = [
    ['../openpower-dump-monitor.service',
     'obmc-host-startmin@0.target.wants/openpower-dump-monitor.service']
]

foreach service: systemd_alias
    meson.add_install_script(
        'sh', '-c',
        'mkdir -p $(dirname $DESTDIR/@0@/@1@)'.format(
            systemd_system_unit_dir, service[1]
        ),
    )
    meson.add_install_script(
        'sh', '-c',
        'ln -s @0@ $DESTDIR/@1@/@2@'.format(
            service[0], systemd_system_unit_dir, service[1]
        ),
    )
endforeach
