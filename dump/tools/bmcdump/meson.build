# SPDX-License-Identifier: Apache-2.0

plugin_dir = meson.current_source_dir() / 'plugins'
plugin_files = files(plugin_dir / 'cfam')
dreport_plugin_dir = join_paths(get_option('prefix'), get_option('datadir'), 'dreport.d/plugins.d')

# Install collected plugin files if any
if plugin_files.length() > 0
    install_data(plugin_files,
        install_dir: dreport_plugin_dir,
        install_mode: 'rwxr-xr-x'
    )
endif

# Copy the Python script to the build directory and set executable permission
configure_file(
    input: 'create_links.py',
    output: 'create_links.py',
    copy: true
)

# Define the path for the configuration file
dreport_conf_dir = join_paths(get_option('prefix'), get_option('datadir'), 'dreport.d/conf.d')
conf_file = meson.current_source_dir() / 'dreport.conf'
install_data(conf_file, install_dir: dreport_conf_dir)


