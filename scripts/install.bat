@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

title DuXun Film Simulation OpenFX v5.0 Installer

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_ROOT=%%~fI"

set "BUILD_OFX=%PROJECT_ROOT%\build\DuXunFilmSim.ofx"
set "BUNDLE_ROOT=C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle"
set "INSTALL_DIR=%BUNDLE_ROOT%\Contents\Win64"
set "INSTALL_OFX=%INSTALL_DIR%\DuXunFilmSim.ofx"

echo.
echo =============================================
echo  DuXun Film Simulation OpenFX v5.0 Installer
echo =============================================
echo.
echo Project root: %PROJECT_ROOT%
echo Build OFX:    %BUILD_OFX%
echo Install OFX:  %INSTALL_OFX%
echo.

if not exist "%BUILD_OFX%" (
    echo [ERROR] Build artifact not found.
    echo Run build_plugin.bat first.
    exit /b 1
)

net session >nul 2>&1
if errorlevel 1 (
    echo [WARN] This script may need an Administrator shell to write Program Files.
)

if not exist "%INSTALL_DIR%\" (
    mkdir "%INSTALL_DIR%" 2>nul
    if errorlevel 1 (
        echo [ERROR] Could not create install directory:
        echo %INSTALL_DIR%
        exit /b 1
    )
)

copy /Y "%BUILD_OFX%" "%INSTALL_OFX%" >nul
if errorlevel 1 (
    echo [ERROR] Copy failed.
    exit /b 1
)

call :sha256 "%BUILD_OFX%" BUILD_HASH
if errorlevel 1 (
    echo [ERROR] Could not hash build artifact.
    exit /b 1
)

call :sha256 "%INSTALL_OFX%" INSTALLED_HASH
if errorlevel 1 (
    echo [ERROR] Could not hash installed artifact.
    exit /b 1
)

echo.
echo Build hash:     !BUILD_HASH!
echo Installed hash: !INSTALLED_HASH!
echo.

if /I not "!BUILD_HASH!"=="!INSTALLED_HASH!" (
    echo [ERROR] Hash mismatch after install.
    exit /b 1
)

echo [OK] Installed OpenFX v5.0 bundle:
echo %BUNDLE_ROOT%
echo.
echo Restart DaVinci Resolve and look for:
echo DuXun - DuXun Film Simulation

exit /b 0

:sha256
set "%~2="
for /f "usebackq delims=" %%H in (`certutil -hashfile "%~1" SHA256 ^| findstr /R /C:"^[0-9A-Fa-f][0-9A-Fa-f]*$"`) do (
    set "%~2=%%H"
    exit /b 0
)
exit /b 1
