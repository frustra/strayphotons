#!/usr/bin/env python3
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
    parser.add_argument("--job-id", help="Job ID to download artifacts for")
    args = parser.parse_args()

    bin_root = os.path.realpath(os.path.join(os.path.dirname(__file__), "../bin"))

    artifacts_url = f"https://api.buildkite.com/v2/organizations/frustra/pipelines/strayphotons/builds/{args.build_num}/jobs/{args.job_id}/artifacts"

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
        os.system('unzip "' + os.path.dirname(local_path) + '"')

    glob_path = os.path.join(bin_root, 'download/*')
    for local_path in glob.iglob(glob_path, recursive=True):
        path = os.path.relpath(local_path, bin_root)
        print("Downloaded artifact: ", path)

if __name__ == '__main__':
    main()
