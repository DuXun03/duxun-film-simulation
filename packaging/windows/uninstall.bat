@echo off
chcp 65001 >nul
setlocal

title DuXun Film Simulation Uninstaller

set "TARGET_BUNDLE=%CommonProgramFiles%\OFX\Plugins\DuXunFilmSim.ofx.bundle"
set "DRY_RUN=0"

if /I "%~1"=="--dry-run" set "DRY_RUN=1"
if /I "%~1"=="/dry-run" set "DRY_RUN=1"

echo.
echo ==============================================
echo  DuXun Film Simulation v5.0 Free Uninstaller
echo ==============================================
echo.
echo Target: %TARGET_BUNDLE%
echo.

if not exist "%TARGET_BUNDLE%\" (
    echo [OK] Plugin is not installed at the standard OpenFX path.
    echo.
    pause
    exit /b 0
)

if "%DRY_RUN%"=="0" (
    net session >nul 2>&1
    if errorlevel 1 (
        echo [INFO] Administrator permission is required.
        echo [INFO] A Windows UAC prompt will open. Please choose Yes.
        echo.
        set "UNINSTALLER_PATH=%~f0"
        set "UNINSTALLER_ARGS=%*"
        powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath $env:UNINSTALLER_PATH -ArgumentList $env:UNINSTALLER_ARGS -Verb RunAs -Wait"
        exit /b %ERRORLEVEL%
    )
)

if "%DRY_RUN%"=="1" (
    echo [DRY RUN] Would remove:
    echo %TARGET_BUNDLE%
    echo.
    exit /b 0
)

rmdir /S /Q "%TARGET_BUNDLE%"
if exist "%TARGET_BUNDLE%\" (
    echo [ERROR] Could not remove plugin bundle. Close DaVinci Resolve and try again.
    echo.
    pause
    exit /b 1
)

echo [OK] Uninstalled.
echo.
pause
exit /b 0
