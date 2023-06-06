#!/usr/bin/env python3
#
# Copyright (C) 2023 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

import argparse
import os
import re
import glob
import sys
import subprocess

include_paths = ["src/", "tests/", "shaders/vulkan", "shaders/lib"]
include_extenions = [".cc", ".hh", ".cpp", ".hpp", ".glsl", ".vert", ".frag", ".geom", ".comp"]
version_pattern = re.compile("version ([0-9]+\.[0-9]+)\.[0-9]+")
allowed_clangformat_versions = ["14.0", "16.0"]

def run_clang_format(filepath, fix):
    if fix:
        modifytime_before = os.path.getmtime(filepath)
        clangformat_output = subprocess.getoutput('clang-format -i "' + filepath + '"')
        modifytime_after = os.path.getmtime(filepath)
        if modifytime_before != modifytime_after:
            print('Formatted file: ' + filepath)
        return True
    else:
        clangformat_output = subprocess.getoutput('clang-format --output-replacements-xml "' + filepath + '"')
        if '<replacement ' in clangformat_output:
            print('Needs format: ' + filepath)
            return False
        else:
            return True

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fix", action='store_true', help="Fix formatting instead of validating.")
    args = parser.parse_args()

    project_root = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))

    try:
        clangformat_version = subprocess.getoutput('clang-format --version').strip()
        clangformat_version = version_pattern.search(clangformat_version).group(1);
        version_allowed = False
        for allowed_version in allowed_clangformat_versions:
            if allowed_version in clangformat_version:
                version_allowed = True
                break
        if not version_allowed:
            print('clang-format version not supported: ' + clangformat_version, file=sys.stderr)
            exit(1)
    except OSError:
        print('Failed to run clang-format', file=sys.stderr)
        raise

    validated = True
    for base_path in include_paths:
        for extension in include_extenions:
            glob_path = os.path.join(project_root, base_path, '**/*' + extension)
            for filepath in glob.iglob(glob_path, recursive=True):
                if not run_clang_format(filepath, args.fix):
                    validated = False

    if not validated:
        exit(1)

if __name__ == '__main__':
    main()
