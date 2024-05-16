subdir('plugins')
subdir('scripts')

# Define the path for the configuration file
dreport_conf_dir = join_paths(get_option('prefix'), get_option('datadir'), 'dreport.d/conf.d')
conf_file = meson.current_source_dir() / 'dreport_op.conf'
install_data(conf_file, install_dir: dreport_conf_dir)
