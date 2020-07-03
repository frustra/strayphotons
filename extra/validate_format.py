#!/usr/bin/env python3

import argparse
import os 
import glob
import sys
import subprocess

include_paths = ["src/", "tests/"]
include_extenions = [".cc", ".hh", ".cpp", ".hpp"]
allowed_clangformat_versions = ["clang-format version 10.0.0"]

def run_clang_format(filepath, fix):
    if fix:
        modifytime_before = os.path.getmtime(filepath)
        clangformat_output = subprocess.check_output(['clang-format', '-i', filepath], text=True)
        modifytime_after = os.path.getmtime(filepath)
        if modifytime_before != modifytime_after:
            print('Formatted file: ' + filepath)
        return True
    else:
        clangformat_output = subprocess.check_output(['clang-format', '--output-replacements-xml', filepath], text=True)
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
        clangformat_version = subprocess.check_output(['clang-format', '--version'], text=True).strip()
        if not clangformat_version in allowed_clangformat_versions:
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
