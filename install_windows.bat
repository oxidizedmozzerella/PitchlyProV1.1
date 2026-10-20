@echo off
REM Usage (Run as Administrator):  install_windows.bat [x64|ARM64]   (default: x64)
setlocal
set ARCH=%1
if "%ARCH%"=="" set ARCH=x64
set SRC=%~dp0build-%ARCH%\PitchlyGraph_artefacts\Release\VST3\PitchlyGraph.vst3
set DST=%CommonProgramFiles%\VST3\PitchlyGraph.vst3
if not exist "%SRC%" (echo Build not found: %SRC% & echo Run build_windows.bat %ARCH% first. & exit /b 1)
xcopy /E /I /Y "%SRC%" "%DST%" || (echo Copy failed - right-click this file and choose "Run as administrator". & exit /b 1)
echo Installed to %DST%
