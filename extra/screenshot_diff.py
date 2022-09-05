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
    
    req = urllib.request.Request("https://api.buildkite.com/v2/organizations/frustra/pipelines/strayphotons/builds?branch=master&state=passed&page=1&per_page=1")
    req.add_header("Authorization", "Bearer " + args.token)
    
    response = urllib.request.urlopen(req).read()
    content = json.loads(response.decode('utf-8'))
    if len(content) == 0:
        print('Could not find valid previous build to compare against')
        exit(0)

    build_info = content[0]
    artifacts = {}

    for job in build_info['jobs']:
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
                        artifacts[artifact['path']]['job'] = job

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

        if not path in artifacts:
            print('Screenshot missing from previous build:', path)
            continue

        current_build_path = os.environ.get('BUILDKITE_BUILD_URL') + '#' + os.environ.get('BUILDKITE_JOB_ID')
        master_build_path = artifacts[path]['job']['web_url']
        master_img_link = 'https://buildkite.com/organizations/frustra/pipelines/strayphotons/builds/' + str(build_info['number'])
        master_img_link += '/jobs/' + artifacts[path]['job']['id'] + '/artifacts/' + artifacts[path]['id']
        master_img_src = 'https://frustra-buildkite.s3.amazonaws.com/' + artifacts[path]['job']['id'] + '/' + path

        os.system('mkdir -p "' + os.path.dirname(diff_path) + '"')
        difference_str = subprocess.getoutput('compare -fuzz 2% -metric mae "' + local_path + '" "' + master_path + '" "' + diff_path + '"')
        metrics = difference_str.split(' ')
        if float(metrics[0]) < 5.0:
            print('Pass', path, ':', float(metrics[0])),
        else:
            print('!! Fail', path, ':', float(metrics[0]))
            subprocess.call('buildkite-agent artifact upload "diff/' + path + '"', shell=True)
            print("\033]1338;url='artifact://diff/" + path + "';alt='diff/" + path + "'\a")
            subprocess.run('buildkite-agent annotate --style "warning" --append', text=True, shell=True,
                input='Screenshot <b>' + path + '</b> has changed: ' + metrics[0] + ' &gt; 5<br/>' +
                '<a href="' + current_build_path + '">Current Build</a> -- ' +
                '<a href="' + master_build_path + '">Master Build</a> -- ' +
                '<a href="artifact://diff/' + path + '">Difference</a><br/>' +
                '<a href="artifact://' + path + '"><img src="artifact://' + path + '" alt="Current Build" height=200></a>' +
                '<a href="' + master_img_link + '"><img src="' + master_img_src + '" alt="Master Build" height=200></a>' +
                '<a href="artifact://diff/' + path + '"><img src="artifact://diff/' + path + '" alt="Difference" height=200></a><br/>'
            )
        

if __name__ == '__main__':
    main()
