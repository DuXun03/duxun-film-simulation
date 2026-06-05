@echo off
setlocal
set "PROJECT_ROOT=%~dp0"
set "WORKSPACE_ROOT=%PROJECT_ROOT%.."
set "OFX_INC=%WORKSPACE_ROOT%\openfx-sdk\include"
set "SRC=%PROJECT_ROOT%ofx\DuXunFilm"
set "OUT=%PROJECT_ROOT%build"
mkdir "%OUT%" 2>nul

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    echo VCVARSALL_FAILED > "%OUT%\build_result.txt"
    exit /b 1
)

cl.exe /nologo /EHsc /O2 /MT /LD /source-charset:utf-8 /execution-charset:utf-8 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DDUXUN_ENABLE_CUDA_RENDER=1 ^
  /I"%OFX_INC%" ^
  /Fo"%OUT%\DuXunFilmSim.obj" ^
  "%SRC%\DuXunFilmSim.cpp" ^
  /Fe"%OUT%\DuXunFilmSim.ofx" ^
  /link /DEF:"%SRC%\DuXunFilmSim.def" /SUBSYSTEM:CONSOLE ^
  > "%OUT%\build_result.txt" 2>&1
set "CL_EXIT=%ERRORLEVEL%"
echo RC=%CL_EXIT% >> "%OUT%\build_result.txt"
if "%CL_EXIT%"=="0" if exist "%OUT%\DuXunFilmSim.ofx" (
    echo BUILD_OK >> "%OUT%\build_result.txt"
) else (
    echo BUILD_FAILED >> "%OUT%\build_result.txt"
    exit /b 1
)
if not "%CL_EXIT%"=="0" (
    echo BUILD_FAILED >> "%OUT%\build_result.txt"
    exit /b %CL_EXIT%
)
