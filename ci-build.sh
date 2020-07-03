#!/bin/bash
format_valid=0
echo -e "--- Running \033[33mclang-format check\033[0m :clipboard:"
if ! ./extra/validate_format.py; then
    echo "^^^ +++"]
    echo -e "\033[31mclang-format validation failed\033[0m"
    format_valid=1
fi

mkdir -p build
if [ -n "$BUILDKITE_ORGANIZATION_SLUG" ]; then
    echo -e "--- Restoring assets cache"
    ASSET_CACHE_PATH=/tmp/buildkite-cache/${BUILDKITE_ORGANIZATION_SLUG}/${BUILDKITE_PIPELINE_SLUG}/sp-assets-cache
    if [ -d ${ASSET_CACHE_PATH} ]; then
        while IFS="" read -r line || [ -n "$line" ]; do
            hash=$(echo $line | awk '{print $1}')
            file=$(echo $line | awk '{print $2}')
            if [ -f ${ASSET_CACHE_PATH}/${hash} ]; then
                echo "Restoring assets/$file"
                mkdir -p assets/${file%/*}
                cp ${ASSET_CACHE_PATH}/${hash} assets/${file}
            fi
        done < assets/asset-list.txt
    fi
fi

echo -e "--- Running \033[33mcmake configure\033[0m :video_game:"
if ! cmake -DCMAKE_BUILD_TYPE=Release -S . -B ./build -GNinja; then
    echo "^^^ +++"]
    echo "\033[31mCMake Configure failed\033[0m"
    exit 1
fi

if [ -n "$ASSET_CACHE_PATH" ]; then
    echo -e "--- Saving assets cache"

    mkdir -p ${ASSET_CACHE_PATH}
    while IFS="" read -r line || [ -n "$line" ]; do
        file=$(echo $line | awk '{print $2}')
        if [ -f assets/${file} ]; then
            hash=$(md5sum assets/${file} | awk '{print $1}')
            cp assets/${file} ${ASSET_CACHE_PATH}/${hash}
        fi
    done < assets/asset-list.txt
fi

echo -e "--- Running \033[33mcmake build\033[0m :rocket:"
if ! cmake --build ./build --config Release --target all; then
    echo "^^^ +++"]
    echo "\033[31mCMake Build failed\033[0m"
    exit 1
fi

echo -e "+++ Running \033[33mtests\033[0m :camera_with_flash:"
cd bin
rm -rf screenshots/*.png

function inline_image {
    if [ -n "$BUILDKITE_ORGANIZATION_SLUG" ]; then
        printf '\033]1338;url='"$1"';alt='"$2"'\a\n'
    else
        echo "Image: $2"
    fi
}

success=0
for file in ../assets/scripts/tests/*.txt; do
    testscript=`realpath --relative-to=../assets/scripts $file`
    echo "Running test: $testscript"
    ./sp-test "$@" "$testscript"
    result=$?
    if [ $result -ne 0 ]; then
        echo -e "\033[31mTest failed with response code: $result\033[0m"
        success=$result
    else
        echo -e "\033[32mTest successful\033[0m"
    fi
    output_path=screenshots/${testscript%.txt}
    mkdir -p $output_path
    for file in screenshots/*.png; do
        mv $file $output_path
        inline_image "artifact://bin/$output_path/${file##*/}" "$output_path/${file##*/}"
    done
done

if [ $success -ne 0 ]; then
    echo -e "\033[31mTest failures detected\033[0m"
    exit $success
fi
if [ $format_valid -ne 0 ]; then
    echo -e "\033[31mclang-format errors detected\033[0m"
    echo -e "Run clang-format with ./extra/validate_format.py --fix"
    exit $format_valid
fi
