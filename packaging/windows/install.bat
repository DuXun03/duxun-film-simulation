@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

title DuXun Film Simulation Installer

set "SCRIPT_DIR=%~dp0"
set "SOURCE_BUNDLE=%SCRIPT_DIR%DuXunFilmSim.ofx.bundle"
set "SOURCE_OFX=%SOURCE_BUNDLE%\Contents\Win64\DuXunFilmSim.ofx"
set "TARGET_ROOT=%CommonProgramFiles%\OFX\Plugins"
set "TARGET_BUNDLE=%TARGET_ROOT%\DuXunFilmSim.ofx.bundle"
set "TARGET_OFX=%TARGET_BUNDLE%\Contents\Win64\DuXunFilmSim.ofx"
set "DRY_RUN=0"

if /I "%~1"=="--dry-run" set "DRY_RUN=1"
if /I "%~1"=="/dry-run" set "DRY_RUN=1"

echo.
echo ============================================
echo  DuXun Film Simulation v5.0 Free Installer
echo ============================================
echo.
echo Source: %SOURCE_BUNDLE%
echo Target: %TARGET_BUNDLE%
echo.

if not exist "%SOURCE_OFX%" (
    echo [ERROR] Package is incomplete. Missing:
    echo %SOURCE_OFX%
    echo.
    pause
    exit /b 1
)

if "%DRY_RUN%"=="0" (
    net session >nul 2>&1
    if errorlevel 1 (
        echo [INFO] Administrator permission is required.
        echo [INFO] A Windows UAC prompt will open. Please choose Yes.
        echo.
        set "INSTALLER_PATH=%~f0"
        set "INSTALLER_ARGS=%*"
        powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath $env:INSTALLER_PATH -ArgumentList $env:INSTALLER_ARGS -Verb RunAs -Wait"
        exit /b %ERRORLEVEL%
    )
)

tasklist /FI "IMAGENAME eq Resolve.exe" 2>nul | find /I "Resolve.exe" >nul
if not errorlevel 1 (
    echo [WARN] DaVinci Resolve appears to be running.
    echo [WARN] Please close DaVinci Resolve before installing.
    echo.
    pause
)

if "%DRY_RUN%"=="1" (
    echo [DRY RUN] Installer package looks valid.
    echo [DRY RUN] No files were copied.
    echo.
    exit /b 0
)

if not exist "%TARGET_ROOT%\" mkdir "%TARGET_ROOT%" 2>nul
if errorlevel 1 (
    echo [ERROR] Could not create target folder:
    echo %TARGET_ROOT%
    echo.
    pause
    exit /b 1
)

robocopy "%SOURCE_BUNDLE%" "%TARGET_BUNDLE%" /MIR /NFL /NDL /NJH /NJS /NP >nul
set "ROBOCOPY_RC=%ERRORLEVEL%"
if %ROBOCOPY_RC% GEQ 8 (
    echo [ERROR] Copy failed. Robocopy exit code: %ROBOCOPY_RC%
    echo.
    pause
    exit /b %ROBOCOPY_RC%
)

call :sha256 "%SOURCE_OFX%" SOURCE_HASH
if errorlevel 1 (
    echo [ERROR] Could not hash source plugin.
    echo.
    pause
    exit /b 1
)

call :sha256 "%TARGET_OFX%" TARGET_HASH
if errorlevel 1 (
    echo [ERROR] Could not hash installed plugin.
    echo.
    pause
    exit /b 1
)

echo Source hash:    !SOURCE_HASH!
echo Installed hash: !TARGET_HASH!
echo.

if /I not "!SOURCE_HASH!"=="!TARGET_HASH!" (
    echo [ERROR] Hash mismatch after installation.
    echo.
    pause
    exit /b 1
)

echo [OK] Installation complete.
echo.
echo Restart DaVinci Resolve, then open:
echo Effects Library ^> OpenFX ^> DuXun ^> DuXun Film Simulation
echo.
pause
exit /b 0

:sha256
set "%~2="
for /f "usebackq delims=" %%H in (`certutil -hashfile "%~1" SHA256 ^| findstr /R /C:"^[0-9A-Fa-f][0-9A-Fa-f]*$"`) do (
    set "%~2=%%H"
    exit /b 0
)
exit /b 1
