#!/usr/bin/env python3

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

if len(sys.argv) == 2:
    if 'CI_CACHE_DIRECTORY' not in os.environ:
        print('ENV[CI_CACHE_DIRECTORY] not set')
        exit(1)

    asset_cache_path = os.environ['CI_CACHE_DIRECTORY'] + '/sp-assets-cache'
    if sys.argv[1] == '--restore':
        if os.path.isdir(asset_cache_path):
            print('Restoring assets from: ' + asset_cache_path)
            with open('assets/asset-list.txt', 'r') as assetList:
                lines = assetList.readlines()
                for line in lines:
                    parts = line.split()
                    if os.path.exists(asset_cache_path + '/' + parts[0]):
                        print('    assets/' + parts[2])
                        dirPath = os.path.dirname('assets/' + parts[2])
                        if not os.path.exists(dirPath):
                            os.makedirs(dirPath)
                        shutil.copyfile(asset_cache_path + '/' + parts[0], 'assets/' + parts[2])
    elif sys.argv[1] == '--save':
        if not os.path.exists(asset_cache_path):
            os.makedirs(asset_cache_path)

        print('Saving assets to: ' + asset_cache_path)
        with open('assets/asset-list.txt', 'r') as assetList:
            lines = assetList.readlines()
            for line in lines:
                parts = line.split()
                if os.path.exists('assets/' + parts[2]):
                    hash = md5sum('assets/' + parts[2])
                    print('    assets/' + parts[2] + '\t' + hash)
                    shutil.copyfile('assets/' + parts[2], asset_cache_path + '/' + hash)
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
