@echo off
chcp 65001 >nul
setlocal

title DuXun Film Simulation OpenFX v5.0 Uninstaller

set "BUNDLE_ROOT=C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle"

echo.
echo ===============================================
echo  DuXun Film Simulation OpenFX v5.0 Uninstaller
echo ===============================================
echo.
echo Bundle path:
echo %BUNDLE_ROOT%
echo.
echo Please close DaVinci Resolve before uninstalling.
echo.

net session >nul 2>&1
if errorlevel 1 (
    echo [WARN] This script may need an Administrator shell to remove files from Program Files.
)

tasklist /FI "IMAGENAME eq Resolve.exe" 2>nul | find /I "Resolve.exe" >nul
if not errorlevel 1 (
    echo [WARN] DaVinci Resolve appears to be running.
    echo Close Resolve before uninstalling so the host releases OFX files.
    echo.
)

if not exist "%BUNDLE_ROOT%\" (
    echo [OK] Bundle is not installed.
    exit /b 0
)

if /I "%~1"=="/q" goto REMOVE_BUNDLE

set "CONFIRM="
set /p CONFIRM=Remove this OFX bundle? Type YES to continue:
if /I not "%CONFIRM%"=="YES" (
    echo [CANCELLED] Uninstall was not changed.
    exit /b 1
)

:REMOVE_BUNDLE
rmdir /S /Q "%BUNDLE_ROOT%"
if exist "%BUNDLE_ROOT%\" (
    echo [ERROR] Could not remove bundle:
    echo %BUNDLE_ROOT%
    exit /b 1
)

echo [OK] Removed DuXun Film Simulation OpenFX v5.0 bundle.
echo If Resolve still lists the plug-in, clear the Resolve OFX cache and restart Resolve.
exit /b 0
