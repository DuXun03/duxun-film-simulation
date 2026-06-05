@echo off
chcp 65001 >nul
REM =============================================
REM  独寻胶片模拟 -- DaVinci Resolve Installer
REM  Windows Version
REM =============================================
REM  Installs the complete plugin package to
REM  DaVinci Resolve, appearing as named effects
REM  in the Effects Library panel.
REM
REM  Usage: Right-click -> Run as Administrator
REM =============================================

setlocal enabledelayedexpansion
title 独寻胶片模拟 -- Resolve Installer

echo.
echo  =============================================
echo   独寻胶片模拟 DuXun Film Simulation v3.0
echo   DaVinci Resolve Plugin Package
echo  =============================================
echo.

REM --- Find Resolve LUT directory ---
set "RESOLVE_LUT=C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\LUT"

if not exist "!RESOLVE_LUT!\" (
    echo  [ERROR] DaVinci Resolve not found!
    echo  Expected: !RESOLVE_LUT!
    echo  Please install DaVinci Resolve first.
    pause
    exit /b 1
)
echo  [OK] Resolve LUT: !RESOLVE_LUT!

REM --- Check admin ---
net session >nul 2>&1
if !errorlevel! neq 0 (
    echo  [WARN] Not running as Administrator - some files may fail
    echo  [WARN] Right-click install.bat -> Run as Administrator
    echo.
)

echo.

REM ==================================================================
REM  Step 1: Install Plugin Package (独立胶片模拟/ folder)
REM ==================================================================
echo  [1/3] Installing 独立胶片模拟 plugin package...
set "PLUGIN_SRC=%~dp0..\plugin\独立胶片模拟"
set "PLUGIN_DST=!RESOLVE_LUT!\独立胶片模拟"

REM Remove old version if exists
if exist "!PLUGIN_DST!\" (
    echo    Removing old installation...
    rmdir /S /Q "!PLUGIN_DST!" 2>nul
)

REM Copy entire plugin folder structure
xcopy "!PLUGIN_SRC!" "!PLUGIN_DST!" /E /I /Y /Q >nul 2>&1

if exist "!PLUGIN_DST!\独立胶片模拟.dctleffect" (
    echo  [OK] Plugin package installed
    echo    Effects Library -> 色彩 -> 独立胶片模拟
) else (
    echo  [ERROR] Plugin installation failed
    pause
    exit /b 1
)

echo.

REM ==================================================================
REM  Step 2: Install Camera IDT LUTs
REM ==================================================================
echo  [2/3] Installing Camera IDT LUTs...
set "SRC_IDT=%~dp0..\presets\idt"
set "LUT_DST=!RESOLVE_LUT!\独立胶片模拟_IDT"
set "LUT_COUNT=0"

if exist "!LUT_DST!\" rmdir /S /Q "!LUT_DST!" 2>nul
mkdir "!LUT_DST!" 2>nul

for %%f in ("!SRC_IDT!\*.cube") do (
    copy /Y "%%f" "!LUT_DST!\%%~nxf" >nul 2>&1
    if !errorlevel! equ 0 set /a LUT_COUNT+=1
)

echo  [OK] !LUT_COUNT! IDT LUTs -> 独立胶片模拟_IDT\
echo.

REM ==================================================================
REM  Step 3: Verify Installation
REM ==================================================================
echo  [3/3] Verifying...
echo.
echo  =============================================
echo   INSTALLED PLUGINS:
echo  =============================================

echo  主插件 (Effects Library -> 色彩):
if exist "!PLUGIN_DST!\独立胶片模拟.dctleffect" echo    [+] 独立胶片模拟 -- 完整四合一管线

echo.
echo  核心管线 (Effects Library -> 独立胶片模拟 -> 核心管线):
if exist "!PLUGIN_DST!\核心管线\对比度曲线.dctleffect" echo    [+] 对比度曲线
if exist "!PLUGIN_DST!\核心管线\色彩矩阵.dctleffect" echo    [+] 色彩矩阵
if exist "!PLUGIN_DST!\核心管线\染料密度.dctleffect" echo    [+] 染料密度
if exist "!PLUGIN_DST!\核心管线\减色色彩头.dctleffect" echo    [+] 减色色彩头

echo.
echo  纹理效果 (Effects Library -> 独立胶片模拟 -> 纹理效果):
if exist "!PLUGIN_DST!\纹理效果\光晕.dctleffect" echo    [+] 光晕
if exist "!PLUGIN_DST!\纹理效果\柔光.dctleffect" echo    [+] 柔光
if exist "!PLUGIN_DST!\纹理效果\胶片颗粒.dctleffect" echo    [+] 胶片颗粒

echo.
echo  进阶色彩 (Effects Library -> 独立胶片模拟 -> 进阶色彩):
if exist "!PLUGIN_DST!\进阶色彩\光谱折射.dctleffect" echo    [+] 光谱折射
if exist "!PLUGIN_DST!\进阶色彩\光散射.dctleffect" echo    [+] 光散射

echo.
echo  显示变换:
if exist "!PLUGIN_DST!\显示变换.dctleffect" echo    [+] 显示变换

echo.
echo  Camera IDT LUTs (!LUT_COUNT! files):
dir /b "!LUT_DST!\*.cube" 2>nul

echo.
echo  =============================================
echo   HOW TO USE:
echo  =============================================
echo.
echo  1. RESTART DaVinci Resolve
echo.
echo  2. Open Effects Library (Effects panel, top-left)
echo     Look for "独立胶片模拟" category
echo.
echo  3. Drag "独立胶片模拟" onto a color node
echo     This gives you the full 14-parameter pipeline
echo.
echo  4. Or drag individual effects:
echo     - 对比度曲线 / 色彩矩阵 / 染料密度 / 减色色彩头
echo     - 光晕 / 柔光 / 胶片颗粒 (texture effects)
echo     - 显示变换 (replaces final CST node)
echo.
echo  5. Camera IDT: OpenFX -> LUT -> 独立胶片模拟_IDT
echo     Select your camera's .cube file
echo.
echo  6. All effects have sliders in the inspector panel
echo     Adjust in real-time on the Color page
echo.
echo  RECOMMENDED NODE CHAIN:
echo    [Camera IDT LUT]
echo        |
echo    [Primary Balance] (Resolve built-in)
echo        |
echo    [独立胶片模拟]   (all-in-one pipeline)
echo        |
echo    [光晕]            (optional texture)
echo        |
echo    [胶片颗粒]        (optional texture)
echo        |
echo    [显示变换]        (replaces CST)
echo        |
echo    [Output]
echo.
echo  =============================================
echo   INSTALLATION COMPLETE
echo   独寻胶片模拟 v3.0
echo  =============================================
echo.
pause
