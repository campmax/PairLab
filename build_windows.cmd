@echo off
setlocal
cd /d "%~dp0"
where cl.exe >nul 2>nul
if not errorlevel 1 goto compiler_ready
set "PL_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%PL_VSWHERE%" goto missing_compiler
for /f "usebackq tokens=*" %%i in (`"%PL_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PL_VSROOT=%%i"
if not defined PL_VSROOT goto missing_compiler
call "%PL_VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto failed
:compiler_ready
if not exist bin mkdir bin
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /bigobj /DNOMINMAX src\main.cpp /Fo"build\main.obj" /Fe"bin\pairlab.exe"
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /bigobj /DNOMINMAX tests\tests.cpp /Fo"build\tests.obj" /Fe"bin\pairlab_tests.exe"
if errorlevel 1 goto failed
bin\pairlab_tests.exe
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /bigobj /DNOMINMAX src\live.cpp /Fo"build\live.obj" /Fe"bin\pairlab_live.exe"
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /bigobj /DNOMINMAX tests\live_tests.cpp /Fo"build\live_tests.obj" /Fe"bin\pairlab_live_tests.exe"
if errorlevel 1 goto failed
bin\pairlab_live_tests.exe tests\live_fixture.json
if errorlevel 1 goto failed
echo BUILD AND TESTS PASSED. Next: RUN_LIVE.cmd, RUN_LIVE_DEMO.cmd or RUN_RESEARCH.cmd
pause
exit /b 0
:missing_compiler
echo Install Desktop development with C++ using Visual Studio Installer.
:failed
echo BUILD FAILED. Read the first error above. Do not run an old executable.
pause
exit /b 1
