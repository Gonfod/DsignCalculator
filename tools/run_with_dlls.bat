@echo off
REM Usage: run_with_dlls.bat [Configuration] [Platform]
set CONFIG=%1
set PLATFORM=%2

if "%CONFIG%"=="" set CONFIG=Debug
if "%PLATFORM%"=="" set PLATFORM=x64

REM Adjust paths if your solution builds elsewhere
set EXE_PATH=%~dp0..\DsignCalculator\%CONFIG%\%PLATFORM%\DsignCalculator.exe
set OUTDIR=%~dp0..\DsignCalculator\%CONFIG%\%PLATFORM%

if not exist "%OUTDIR%" (
 echo Output folder not found: %OUTDIR%
 goto :eof
)

powershell -ExecutionPolicy Bypass -File "%~dp0copy_sfml_dlls.ps1" -OutDir "%OUTDIR%"

if exist "%EXE_PATH%" (
 echo Running %EXE_PATH%
 pushd "%OUTDIR%"
 "%EXE_PATH%"
 popd
) else (
 echo Executable not found: %EXE_PATH%
)
