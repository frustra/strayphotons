#!/bin/bash
format_valid=0
echo -e "--- Running \033[33mclang-format check\033[0m :clipboard:"
if ! ./extra/validate_format.py; then
    echo -e "^^^ +++"
    echo -e "\033[31mclang-format validation failed\033[0m"
    format_valid=1
fi

mkdir -p build
if [ -n "$CI_CACHE_DIRECTORY" ]; then
    echo -e "--- Restoring assets cache"
    ./assets/cache-assets.py --restore
fi

echo -e "--- Running \033[33mcmake configure\033[0m :video_game:"
if ! cmake -DCMAKE_BUILD_TYPE=Release -S . -B ./build -GNinja; then
    echo -e "\n^^^ +++"
    echo -e "\033[31mCMake Configure failed\033[0m"
    exit 1
fi

if [ -n "$CI_CACHE_DIRECTORY" ]; then
    echo -e "--- Saving assets cache"
    ./assets/cache-assets.py --save
fi

echo -e "--- Running \033[33mcmake build\033[0m :rocket:"
cmake --build ./build --config Release --target all 2>&1 | tee >(grep -E "error( \w+)?:" > ./build/build_errors.log)
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    echo -e "\n^^^ +++"
    echo -e "\033[31mCMake Build failed\033[0m"
    cat <(echo '```term') ./build/build_errors.log <(echo "") <(echo '```') | buildkite-agent annotate --style error --append
    exit 1
fi

success=0
echo -e "--- Running \033[33munit tests\033[0m :clipboard:"
cd bin
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

echo -e "+++ Running \033[33mtest scripts\033[0m :camera_with_flash:"
rm -rf screenshots/*.png

function inline_image {
    if [ -n "$BUILDKITE_ORGANIZATION_SLUG" ]; then
        printf '\033]1338;url='"$1"';alt='"$2"'\a\n'
    else
        echo "Image: $2"
    fi
}

rm -rf traces
mkdir -p traces/tests/

for file in ../assets/scripts/tests/*.txt; do
    testscript=`realpath --relative-to=../assets/scripts $file`
    trace_path=traces/${testscript%.txt}.tracy
    ../extra/Tracy-capture -a 127.0.0.1 -o "$trace_path" 1>/dev/null &

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
        buildkite-agent artifact upload "$output_path/${file##*/}"
        inline_image "artifact://$output_path/${file##*/}" "$output_path/${file##*/}"
    done
    [[ -f "$trace_path" ]] && buildkite-agent artifact upload "$trace_path"
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

if [ -n "$BUILDKITE_API_TOKEN" ]; then
    echo -e "--- Comparing screenshots :camera_with_flash:"
    ../extra/validate_format.py --token "$BUILDKITE_API_TOKEN"
fi

echo -e "\033[92mTests succeeded\033[0m"
