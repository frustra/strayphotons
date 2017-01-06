#!/bin/sh
set -e
#cmd /k windows_physx.bat # this doesn't work on all windows shells, run windows_physx.bat manually first
rm -rf vendor/lib/physx
mkdir -p vendor/lib/physx
cp ext/physx/PhysXSDK/Lib/vc14win32/*.lib vendor/lib/physx
cp ext/physx/PhysXSDK/Bin/vc14win32/*.dll build
