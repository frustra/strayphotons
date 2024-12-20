@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
"C:\Program Files\Git\bin\bash.exe" -c "./ci-build.sh"
