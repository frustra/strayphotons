#!/bin/bash
#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

format_valid=0
echo -e "--- Running \033[33mclang-format check\033[0m :clipboard:"
if ! ./extra/validate_format.py; then
    echo -e "^^^ +++"
    echo -e "\033[31mclang-format validation failed\033[0m"
    format_valid=1
fi

mkdir -p build
if [ -n "$CI_CACHE_DIRECTORY" ]; then
    echo -e "~~~ Restoring assets cache"
    ./assets/cache-assets.py --restore
    
    if [ -d "$CI_CACHE_DIRECTORY/sp-physics-cache" ]; then
        echo -e "~~~ Restoring physics collision cache"
        mkdir -p ./assets/cache
        cp -r "$CI_CACHE_DIRECTORY/sp-physics-cache/collision" ./assets/cache/
    fi
fi

echo -e "--- Running \033[33mcmake configure\033[0m :video_game:"
if [ "$CI_PACKAGE_RELEASE" = "1" ]; then
    if ! cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSP_PACKAGE_RELEASE=1 -S . -B ./build -GNinja; then
        echo -e "\n^^^ +++"
        echo -e "\033[31mCMake Configure failed\033[0m"
        exit 1
    fi
else
    if ! cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -B ./build -GNinja; then
        echo -e "\n^^^ +++"
        echo -e "\033[31mCMake Configure failed\033[0m"
        exit 1
    fi
fi

if [ -n "$CI_CACHE_DIRECTORY" ]; then
    echo -e "~~~ Saving assets cache"
    ./assets/cache-assets.py --save
fi

echo -e "--- Running \033[33mcmake build\033[0m :rocket:"
cmake --build ./build --config RelWithDebInfo --target all 2>&1 | tee >(grep -E "error( \w+)?:" > ./build/build_errors.log)
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    echo -e "\n^^^ +++"
    echo -e "\033[31mCMake Build failed\033[0m"
    if [ -n "$BUILDKITE_BRANCH" ]; then
        cat <(echo '```term') ./build/build_errors.log <(echo "") <(echo '```') | buildkite-agent annotate --style error --append
    fi
    exit 1
fi

cd bin

success=0
if ! [ "$CI_PACKAGE_RELEASE" = "1" ]; then
    echo -e "--- Running \033[33munit tests\033[0m :clipboard:"
    ./sp-unit-tests
    result=$?
    if [ $result -ne 0 ]; then
        echo -e "\n^^^ +++"
        echo -e "\033[31mTest failed with response code: $result\033[0m"
        success=$result
    else
        echo -e "\033[32mTest successful\033[0m"
    fi

    echo -e "--- Running \033[33mintegration tests\033[0m :clipboard:"
    ./sp-integration-tests
    result=$?
    if [ $result -ne 0 ]; then
        echo -e "\n^^^ +++"
        echo -e "\033[31mTest failed with response code: $result\033[0m"
        success=$result
    else
        echo -e "\033[32mTest successful\033[0m"
    fi

    echo -e "--- Running \033[33mtest scripts\033[0m :camera_with_flash:"
    rm -rf screenshots/*.png

    function inline_image {
        if [ -n "$BUILDKITE_BRANCH" ]; then
            printf '\033]1338;url='"$1"';alt='"$2"'\a\n'
        else
            echo "Image: $2"
        fi
    }

    rm -rf traces
    mkdir -p traces/tests/

    core_dumped=0
    for testfile in ../assets/scripts/tests/*.txt; do
        testscript=`realpath --relative-to=../assets/scripts $testfile`
        trace_path=traces/${testscript%.txt}.tracy
        ../extra/Tracy-capture -a 127.0.0.1 -o "$trace_path" 1>/dev/null &

        echo "Running test: $testscript"
        ./sp-test "$@" --run "$testscript"
        result=$?
        if [ $result -ne 0 ]; then
            echo -e "\n^^^ +++"
            echo -e "\033[31mTest failed with response code: $result\033[0m"
            success=$result
        else
            echo -e "\033[32mTest successful\033[0m"
        fi
        output_path=screenshots/${testscript%.txt}
        mkdir -p $output_path
        for file in screenshots/*.png; do
            mv $file $output_path
            [ -n "$BUILDKITE_BRANCH" ] && buildkite-agent artifact upload "$output_path/${file##*/}"
            inline_image "artifact://$output_path/${file##*/}" "$output_path/${file##*/}"
        done
        [ -n "$BUILDKITE_BRANCH" ] && [[ -f "$trace_path" ]] && buildkite-agent artifact upload "$trace_path"
        if [ -n "$BUILDKITE_BRANCH" ] && [[ -f ./core ]]; then
            echo -e "\033[31mUploading core dump\033[0m"
            core_dumped=1
            testname="${testfile##*/}"
            testname="${testname%.*}"
            mv ./core "./core-$testname"
            buildkite-agent artifact upload "./core-$testname"
        fi
    done
    if [ $core_dumped -ne 0 ]; then
        buildkite-agent artifact upload ./sp-test
    fi
fi

if [ $success -eq 0 ] && [ -n "$CI_CACHE_DIRECTORY" ]; then
    echo -e "~~~ Saving physics collision cache"
    mkdir -p "$CI_CACHE_DIRECTORY/sp-physics-cache"

    # Delete cache files older than 30 days so any removed models don't stick around forever
    find "$CI_CACHE_DIRECTORY/sp-physics-cache" -type f -mtime 30 -delete
    cp -r ../assets/cache/collision "$CI_CACHE_DIRECTORY/sp-physics-cache/"
fi

if [ "$CI_PACKAGE_RELEASE" = "1" ]; then
    echo -e "--- Uploading package release :arrow_up:"
    buildkite-agent artifact upload "assets.spdata"
    buildkite-agent artifact upload "openvr_api.dll"
    buildkite-agent artifact upload "actions.json"
    buildkite-agent artifact upload "sp-vk.exe"
    buildkite-agent artifact upload "sp-vk.pdb"
    buildkite-agent artifact upload "scripts/*"
elif [ -n "$BUILDKITE_API_TOKEN" ]; then
    echo -e "--- Comparing screenshots :camera_with_flash:"
    ../extra/screenshot_diff.py --token "$BUILDKITE_API_TOKEN"
fi

if [ $success -ne 0 ]; then
    echo -e "\n+++ \033[31mTest failures detected\033[0m"
    exit $success
fi
if [ $format_valid -ne 0 ]; then
    echo -e "\n+++ \033[31mclang-format errors detected\033[0m"
    echo -e "Run clang-format with ./extra/validate_format.py --fix"
    exit $format_valid
fi

echo -e "\n+++ \033[92mTests succeeded\033[0m"
