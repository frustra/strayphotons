#!/bin/bash
cd bin

success=0
for file in ../assets/scripts/tests/*.txt; do
    testscript=`realpath --relative-to=../assets/scripts $file`
    echo "Running test: $testscript"
    ./sp-test "$@" "$testscript"
    result=$?
    if [ $result -ne 0 ]; then
        echo "Test failed with response code: $result"
        success=$result
    else
        echo "Test successful"
    fi
done

if [ $success -ne 0 ]; then
    echo "Test failures detected"
    exit $success
fi
