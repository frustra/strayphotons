#!/usr/bin/env -S python3 -u
#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

import os
import shutil
import sys
import hashlib

def md5sum(filename):
    hash = hashlib.md5()
    with open(filename, 'rb') as file:
        for chunk in iter(lambda: file.read(4096), b""):
            hash.update(chunk)
    return hash.hexdigest()

if __name__ == '__main__':
    assets_root = os.path.realpath(os.path.join(os.path.dirname(__file__), "../assets"))

    if len(sys.argv) == 2:
        if 'CI_CACHE_DIRECTORY' not in os.environ:
            print('ENV[CI_CACHE_DIRECTORY] not set')
            exit(1)

        asset_cache_path = os.environ['CI_CACHE_DIRECTORY'] + '/sp-assets-cache'
        if sys.argv[1] == '--restore':
            if os.path.isdir(asset_cache_path):
                print('Restoring assets from: ' + asset_cache_path)
                with open(os.path.join(assets_root, 'asset-list.txt'), 'r') as assetList:
                    lines = assetList.readlines()
                    for line in lines:
                        parts = line.split()
                        asset_path = os.path.join(assets_root, parts[2])
                        if os.path.exists(asset_cache_path + '/' + parts[0]):
                            print('    assets/' + parts[2])
                            dirPath = os.path.dirname(asset_path)
                            if not os.path.exists(dirPath):
                                os.makedirs(dirPath)
                            shutil.copyfile(asset_cache_path + '/' + parts[0], asset_path)
        elif sys.argv[1] == '--save':
            if not os.path.exists(asset_cache_path):
                os.makedirs(asset_cache_path)

            print('Saving assets to: ' + asset_cache_path)
            with open(os.path.join(assets_root, 'asset-list.txt'), 'r') as assetList:
                lines = assetList.readlines()
                for line in lines:
                    parts = line.split()
                    if os.path.exists(asset_path):
                        hash = md5sum(asset_path)
                        print('    assets/' + parts[2] + '\t' + hash)
                        shutil.copyfile(asset_path, asset_cache_path + '/' + hash)
        else:
            print('Usage:')
            print('    ' + sys.argv[0] + ' --save')
            print('    ' + sys.argv[0] + ' --restore')
            exit(1)
    else:
        print('Usage:')
        print('    ' + sys.argv[0] + ' --save')
        print('    ' + sys.argv[0] + ' --restore')
        exit(1)
