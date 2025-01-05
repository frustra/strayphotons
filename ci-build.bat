@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
"C:\Program Files\Git\bin\bash.exe" -c "./ci-build.sh"
