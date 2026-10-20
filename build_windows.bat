@echo off
REM Usage:  build_windows.bat [x64|ARM64]      (default: x64)
REM Needs:  Visual Studio 2022+ with "Desktop development with C++" (includes CMake), and git.
setlocal
set ARCH=%1
if "%ARCH%"=="" set ARCH=x64

where cmake >nul 2>nul || (echo CMake not found. Open "Developer Command Prompt for VS" or install CMake from cmake.org. & exit /b 1)

echo.
echo === Building PitchlyGraph for %ARCH% (first build downloads JUCE, takes a few minutes) ===
cmake -S . -B build-%ARCH% -A %ARCH% || exit /b 1
cmake --build build-%ARCH% --config Release --parallel || exit /b 1

echo.
echo === Done ===
echo Plugin: %CD%\build-%ARCH%\PitchlyGraph_artefacts\Release\VST3\PitchlyGraph.vst3
echo Run install_windows.bat %ARCH%  (as Administrator) to copy it where FL Studio looks.
