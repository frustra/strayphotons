#!/usr/bin/env python3

import argparse
import os
import glob
import sys
import subprocess
import urllib.request
import json
import shutil

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--token", help="Buildkite API token")
    args = parser.parse_args()

    bin_root = os.path.realpath(os.path.join(os.path.dirname(__file__), "../bin"))
    
    req = urllib.request.Request("https://api.buildkite.com/v2/builds?branch=master&state=passed&page=1&per_page=1")
    req.add_header("Authorization", "Bearer " + args.token)
    
    response = urllib.request.urlopen(req).read()
    content = json.loads(response.decode('utf-8'))
    artifacts = {}

    for job in content[0]['jobs']:
        if job['state'] != "passed":
            continue
        
        for rule in job['agent_query_rules']:
            parts = rule.split('=')
            env_value = os.environ.get('BUILDKITE_AGENT_META_DATA_' + parts[0].upper())
            if env_value != parts[1]:
                break
        else:
            print("Using Job:", job['artifacts_url'])
            page = 1
            while len(content) > 0 or page == 1:
                req = urllib.request.Request(job['artifacts_url'] + '?page=' + str(page))
                req.add_header("Authorization", "Bearer " + args.token)
                
                response = urllib.request.urlopen(req).read()
                content = json.loads(response.decode('utf-8'))

                for artifact in content:
                    if artifact['path'].startswith('screenshots/tests/'):
                        artifacts[artifact['path']] = artifact

                page += 1

    print('Downloading', len(artifacts), 'artifacts...')

    for path in artifacts:
        remote_path = artifacts[path]['download_url']
        local_path = os.path.join(bin_root, 'comparison/' + path)
    
        os.system('mkdir -p "' + os.path.dirname(local_path) + '"')
        os.system('curl -s -L -H "Authorization: Bearer ' + args.token + '" "' + remote_path + '" --output "' + local_path + '"')

    glob_path = os.path.join(bin_root, 'screenshots/tests/**/*.png')
    for local_path in glob.iglob(glob_path, recursive=True):
        path = os.path.relpath(local_path, bin_root)
        master_path = os.path.join(bin_root, 'comparison/' + path)
        diff_path = os.path.join(bin_root, 'diff/' + path)

        os.system('mkdir -p "' + os.path.dirname(diff_path) + '"')
        difference_str = subprocess.getoutput('compare -fuzz 2% -metric mae "' + local_path + '" "' + master_path + '" "' + diff_path + '"')
        metrics = difference_str.split(' ')
        if float(metrics[0]) < 10.0:
            print('Pass', path, ':', float(metrics[0])),
        else:
            print('!! Fail', path, ':', float(metrics[0]))
            subprocess.call('buildkite-agent artifact upload "diff/' + path + '"', shell=True)
            print("\033]1338;url='artifact://diff/" + path + "';alt='diff/" + path + "'\a")
            subprocess.run('buildkite-agent annotate --style "warning"', text=True, shell=True,
                input='Screenshot <b>' + path + '</b> has changed: ' + metrics[0] + ' &gt; 10<br/><a href="artifact://diff/' + path + '"><img src="artifact://diff/' + path + '" alt="diff/' + path + '" height=250></a>')
        

if __name__ == '__main__':
    main()
