@echo off
chcp 65001 >nul
echo ============================================
echo  独寻胶片模拟 DCTL 安装
echo ============================================
echo.

set "SRC=%~dp0dctl"
set "LUT_SRC=%~dp0presets\film_stocks"
set "DST=C:/ProgramData/Blackmagic Design\DaVinci Resolve\Support\LUT"
set "DST_PLUGIN=%DST%\独寻胶片模拟"
set "DST_IDT=%DST%\独寻胶片模拟_IDT"

if not exist "%DST%\" (
    echo [ERROR] DaVinci Resolve not found at %DST%
    pause & exit /b 1
)

echo [1/3] Copying DCTL shaders...
mkdir "%DST_PLUGIN%" 2>nul
copy /Y "%SRC%\DuXun-*.dctl" "%DST_PLUGIN%\" >nul
copy /Y "%SRC%\DuXun-*.h"   "%DST_PLUGIN%\" >nul
echo   Done.

echo [2/3] Copying 65 film stock LUTs...
mkdir "%DST_PLUGIN%\LUTs" 2>nul
copy /Y "%LUT_SRC%\*.cube" "%DST_PLUGIN%\LUTs\" >nul
echo   Done.

echo [3/3] Copying presets header...
copy /Y "%LUT_SRC%\film_presets.h" "%DST_PLUGIN%\" >nul
echo   Done.

echo.
echo ============================================
echo  INSTALL COMPLETE
echo ============================================
echo.
echo In DaVinci Resolve:
echo   Color page -^> OpenFX panel -^> search "DCTL"
echo   Drag DCTL onto a node
echo   Select "DuXun-Core" from dropdown
echo   65 film stock LUTs in DuXun-Core dropdown
echo.
pause
