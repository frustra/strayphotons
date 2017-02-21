call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
msbuild ext\physx\PhysXSDK\Source\compiler\vc14win32\PhysX.sln /p:Configuration=Release
exit
