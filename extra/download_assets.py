#!/usr/bin/env -S python3 -u
#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

import os
import shutil
import hashlib
import urllib.request

def md5sum(filename):
    hash = hashlib.md5()
    with open(filename, 'rb') as file:
        for chunk in iter(lambda: file.read(4096), b""):
            hash.update(chunk)
    return hash.hexdigest()

if __name__ == '__main__':
    assets_root = os.path.realpath(os.path.join(os.path.dirname(__file__), "../assets"))

    print('Checking for missing assets...')
    with open(os.path.join(assets_root, 'asset-list.txt'), 'r') as assetList:
        lines = assetList.readlines()
        for line in lines:
            parts = line.split()
            asset_path = os.path.join(assets_root, parts[2])
            if os.path.exists(asset_path):
                hash = md5sum(asset_path)
                if hash == parts[0]:
                    continue
            asset_url = parts[1] + '?request_md5=' + parts[0]
            print('    Downloading assets/' + parts[2] + ' from ' + parts[1])
            dirPath = os.path.dirname(asset_path)
            if not os.path.exists(dirPath):
                os.makedirs(dirPath)

            req = urllib.request.Request(asset_url)
            req.add_header('User-Agent', 'strayphotons-asset-downloader.py/1.0')
            content = urllib.request.urlopen(req).read()
            with open(asset_path, 'wb') as f:
                f.write(content)
            hash = md5sum(asset_path)
            if hash != parts[0]:
                print('    Asset hash mismatch: ' + asset_url)
                os.exit(1)
