#!/usr/bin/env -S python3 -u
#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

import os
import subprocess

ext_dependencies = ["glm", "magic_enum", "robin-hood-hashing", "Tecs"]

project_root = os.path.realpath(os.path.join(os.path.dirname(__file__), "../"))
sdk_dir = os.path.join(project_root, "sdk")
output_dir = os.path.join(project_root, "build/archive")
archive_path = os.path.join(output_dir, 'strayphotons_sdk.zip')
ext_dir = os.path.join(output_dir, "sdk/ext")

if __name__ == "__main__":
    os.system(f'bash -c \'rm -rf "{output_dir}"\'')
    os.system(f'bash -c \'mkdir -p "{ext_dir}"\'')
    os.system(f'bash -c \'cp -r "{sdk_dir}" "{output_dir}"\'')

    for dep in ext_dependencies:
        url = subprocess.check_output(
            f"git config --file=.gitmodules submodule.ext/{dep}.url",
            stderr=subprocess.STDOUT,
            shell=True,
            cwd=project_root,
        ).decode().strip()
        commit = subprocess.check_output(
            f"git submodule status ext/{dep}",
            stderr=subprocess.STDOUT,
            shell=True,
            cwd=project_root,
        ).decode().strip().split(" ", 2)[0]
        print("URL:", url)
        print("Commit:", commit)
        os.system(f"git clone --depth 1 --revision {commit} {url} {os.path.join(ext_dir, dep)}")

    os.system(f'bash -c "cd {output_dir}; zip -r {archive_path} sdk"')
    os.system(f'buildkite-agent artifact upload "{archive_path}"')
