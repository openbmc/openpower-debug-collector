#!/usr/bin/env python3

import os
import re
import configparser


def install_dreport_user_script(script_path, dreport_dir, plugin_dir, conf_file):
    configure = configparser.ConfigParser()
    configure.read(conf_file)
    config = "config:"
    section = "DumpType"

    script = os.path.basename(script_path)

    with open(script_path, "r") as file:
        for line in file:
            if config not in line:
                continue
            revalue = re.search(r"[0-9]+\s[0-9]+", line)
            if not revalue:
                continue
            parse_value = revalue.group(0)
            config_values = re.split(r"\s+", parse_value)
            if len(config_values) != 2:
                break
            priority = config_values[1]
            types = [int(d) for d in str(config_values[0])]
            for type in types:
                if not configure.has_option(section, str(type)):
                    continue
                typestr = configure.get(section, str(type))
                destdir = os.path.join(dreport_dir, f"pl_{typestr}.d")
                os.makedirs(destdir, exist_ok=True)
                linkname = f"E{priority}_{script}"
                destlink = os.path.join(destdir, linkname)
                if not os.path.exists(destlink):
                    os.symlink(os.path.relpath(script_path, start=destdir), destlink)


def main():
    import sys

    if len(sys.argv) != 4:
        print("Usage: create_links.py <dreport_dir> <plugin_dir> <conf_file>")
        sys.exit(1)
    dreport_dir = sys.argv[1]
    plugin_dir = sys.argv[2]
    conf_file = sys.argv[3]

    # Find all plugin files
    plugin_files = [
        os.path.join(plugin_dir, f)
        for f in os.listdir(plugin_dir)
        if os.path.isfile(os.path.join(plugin_dir, f))
    ]

    for plugin_file in plugin_files:
        install_dreport_user_script(plugin_file, dreport_dir, plugin_dir, conf_file)


if __name__ == "__main__":
    main()
