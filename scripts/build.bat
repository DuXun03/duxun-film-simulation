@echo off
chcp 65001 >nul
echo ============================================
echo  独寻胶片模拟 OFX Plugin Build
echo ============================================
echo.

set "VSINSTALL=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat"
set "OFX_SDK=%~dp0..\openfx-sdk"
set "SRC=%~dp0..\ofx\DuXunFilm"
set "PRESETS=%~dp0..\presets\film_stocks"
set "OUT=%~dp0..\build"

if not exist "%VCVARS%" (
    echo [ERROR] vcvarsall.bat not found at %VCVARS%
    pause & exit /b 1
)

if not exist "%OFX_SDK%\include\ofxCore.h" (
    echo [ERROR] OpenFX SDK not found at %OFX_SDK%
    pause & exit /b 1
)

echo [1/2] Setting up MSVC environment...
call "%VCVARS%" x64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC
    pause & exit /b 1
)

echo [2/2] Compiling DuXunFilmSim.ofx...

mkdir "%OUT%" 2>nul

cl.exe /nologo /EHsc /O2 /MD /DLITTLE_ENDIAN ^
    /I"%OFX_SDK%\include" ^
    /I"%OFX_SDK%\Support\include" ^
    /I"%SRC%" ^
    /I"%PRESETS%" ^
    /DDUXUN_FILM_PLUGIN_EXPORTS ^
    /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
    /Fe"%OUT%\DuXunFilmSim.ofx" ^
    "%SRC%\DuXunPlugin.cpp" ^
    "%SRC%\DuXunPluginFactory.cpp" ^
    "%SRC%\DuXunProcess.cpp" ^
    "%OFX_SDK%\Support\Library\ofxsCore.cpp" ^
    "%OFX_SDK%\Support\Library\ofxsImageEffect.cpp" ^
    "%OFX_SDK%\Support\Library\ofxsInteract.cpp" ^
    "%OFX_SDK%\Support\Library\ofxsLog.cpp" ^
    "%OFX_SDK%\Support\Library\ofxsMultiThread.cpp" ^
    "%OFX_SDK%\Support\Library\ofxsParams.cpp" ^
    "%OFX_SDK%\Support\Library\ofxsProperty.cpp" ^
    "%OFX_SDK%\Support\Library\ofxsPropertyValidation.cpp" ^
    /link /DLL /DEF:"%SRC%\DuXunFilmSim.def" /OUT:"%OUT%\DuXunFilmSim.ofx" 2>&1

if errorlevel 1 (
    echo.
    echo [ERROR] Compilation failed
    pause & exit /b 1
)

echo.
echo ============================================
echo  BUILD SUCCESS
echo  Output: %OUT%\DuXunFilmSim.ofx
echo ============================================
pause
