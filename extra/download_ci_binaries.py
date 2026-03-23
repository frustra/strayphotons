#!/usr/bin/env -S python3 -u
#
# Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

import argparse
import os
import glob
import subprocess
import urllib.request
import json

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--token", help="Buildkite API token")
    parser.add_argument("--build-num", help="Build number to download artifacts for")
    args = parser.parse_args()

    bin_root = os.path.realpath(os.path.join(os.path.dirname(__file__), "../bin"))

    req = urllib.request.Request(f"https://api.buildkite.com/v2/organizations/frustra/pipelines/strayphotons/builds/{args.build_num}")
    req.add_header("Authorization", "Bearer " + args.token)

    response = urllib.request.urlopen(req).read()
    content = json.loads(response.decode('utf-8'))
    if len(content) == 0:
        print('Could not find build to download artifacts from:', req.url)
        exit(1)

    artifacts_url = None
    for job in content['jobs']:
        if job['state'] != "passed":
            continue

        if 'linux=1' not in job['agent']['meta_data']:
            continue

        artifacts_url = job['artifacts_url']
        break

    if artifacts_url is None:
        print('Could not find job to download artifacts from:', content['jobs'])
        exit(1)

    content = ""
    artifacts = {}

    print("Using Job:", artifacts_url)
    page = 1
    while len(content) > 0 or page == 1:
        req = urllib.request.Request(artifacts_url + '?page=' + str(page))
        req.add_header("Authorization", "Bearer " + args.token)

        response = urllib.request.urlopen(req).read()
        content = json.loads(response.decode('utf-8'))

        for artifact in content:
            if artifact['path'].startswith('sp_') and artifact['path'].endswith('.zip'):
                artifacts[artifact['path']] = artifact

        page += 1

    print('Downloading', len(artifacts), 'artifacts...')

    for path in artifacts:
        remote_path = artifacts[path]['download_url']
        local_path = os.path.join(bin_root, 'download/' + path)

        os.system('bash -c \'mkdir -p "' + os.path.dirname(local_path) + '"\'')
        os.system('curl -s -L -H "Authorization: Bearer ' + args.token + '" "' + remote_path + '" --output "' + local_path + '"')
        os.system('unzip -o "' + local_path + '" -d "' + os.path.join(bin_root, 'download') + '"')

    os.system('bash -c \'mv "' + os.path.join(bin_root, 'download') + '"/*/* "' + bin_root + '"\'')

if __name__ == '__main__':
    main()
