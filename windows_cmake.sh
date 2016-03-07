#!/bin/bash
mkdir -p build
rmdir build/assets 2> /dev/null
cmd <<< "mklink /D \".\\build\\assets\" \"..\\assets\"" > /dev/null
cd build; cmake -G "Visual Studio 14" ..
