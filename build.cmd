@echo off
setlocal
cd /d "%~dp0"
set "TEMP=%~dp0build-temp"
set "TMP=%TEMP%"
if not exist "%TEMP%" mkdir "%TEMP%"
if exist "%~dp0.tools\msvc\setup_x86.bat" set "PORTABLE_MSVC=%~dp0.tools\msvc"
if defined PORTABLE_MSVC goto compiler_ready
if defined VSROOT goto compiler_ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto compiler_missing
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vsroot.txt"
set /p VSROOT=<"%TEMP%\vsroot.txt"
:compiler_ready
if defined PORTABLE_MSVC goto build_all
if not exist "%VSROOT%\VC\Auxiliary\Build\vcvarsall.bat" goto compiler_missing
:build_all
call :build x86
if errorlevel 1 exit /b 1
call :build x64
exit /b %errorlevel%
:build
if defined PORTABLE_MSVC (
call "%PORTABLE_MSVC%\setup_%1.bat"
) else (
call "%VSROOT%\VC\Auxiliary\Build\vcvarsall.bat" %1
)
if errorlevel 1 exit /b 1
if not exist "bin\%1" mkdir "bin\%1"
pushd "bin\%1"
cl /nologo /std:c++17 /EHsc /O2 /MT /utf-8 /W3 /LD ..\..\src\CodexQuota.cpp /Fe:CodexQuota.dll /link /DEF:..\..\src\CodexQuota.def user32.lib gdi32.lib shell32.lib
if errorlevel 1 (popd & exit /b 1)
cl /nologo /std:c++17 /EHsc /O2 /MT /utf-8 ..\..\src\tests.cpp /Fe:QuotaTests.exe /link user32.lib gdi32.lib shell32.lib
if errorlevel 1 (popd & exit /b 1)
QuotaTests.exe CodexQuota.dll
if errorlevel 1 (popd & exit /b 1)
cl /nologo /std:c++17 /EHsc /O2 /MT /utf-8 ..\..\src\render_tests.cpp /Fe:RenderTests.exe /link user32.lib gdi32.lib shell32.lib
if errorlevel 1 (popd & exit /b 1)
RenderTests.exe
if errorlevel 1 (popd & exit /b 1)
popd
exit /b 0

:compiler_missing
echo MSVC C++ Build Tools not found. Install them or set VSROOT to the installation directory.
exit /b 1
