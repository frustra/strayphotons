#!/usr/bin/env python3
#
# Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

import argparse
import os
import glob
import subprocess

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
    parser = argparse.ArgumentParser(description="Removes or renames in-use target outputs so new changes can be picked up upon reopening the files.")
    parser.add_argument("target", help="The target name to rename or remove")
    parser.add_argument("extension", nargs="+", help="The target extension types to rename or remove")
    args = parser.parse_args()

    bin_root = os.path.realpath(os.path.join(os.path.dirname(__file__), "../bin"))

    for ext in args.extension:
        target_file = f'{args.target}.{ext}'
        target_path = os.path.join(bin_root, target_file)
        old_glob_path = os.path.join(bin_root, f'{args.target}_old*.{ext}')
        for filepath in glob.iglob(old_glob_path):
            # print(f'Removing: {filepath}')
            try:
                os.remove(filepath)
                # print(f'Removed: {os.path.basename(filepath)}')
            except Exception as err:
                # print("Target file in use: ", filepath)
                # print("Error: ", err)
                pass
        if os.path.exists(target_path):
            try:
                os.remove(target_path)
                # print(f'Removed old: {target_file}')
            except Exception as err:
                # print("Target file in use: ", target_path)
                # print("Error: ", err)
                i = 0
                while True:
                    rename_file = f'{args.target}_old{i}.{ext}'
                    rename_path = os.path.join(bin_root, rename_file)
                    if not os.path.exists(rename_path):
                        break
                    i += 1
                print(f'Renaming in-use: {target_file} to {rename_file}')
                os.rename(target_path, rename_path)

if __name__ == '__main__':
    main()
