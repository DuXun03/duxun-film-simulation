@echo off
REM ============================================================
REM Compile DuXunFilmSim.cpp into DuXunFilmSim.ofx (DLL)
REM ============================================================

setlocal
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_ROOT=%%~fI\"
for %%I in ("%PROJECT_ROOT%..") do set "WORKSPACE_ROOT=%%~fI"
set "OFX_INC=%WORKSPACE_ROOT%\openfx-sdk\include"
set "SRC=%PROJECT_ROOT%ofx\DuXunFilm"
set "OUT=%PROJECT_ROOT%build"

mkdir "%OUT%" 2>nul

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 goto :fail

echo === Compiling DuXunFilmSim.cpp ===
cl.exe /nologo /EHsc /O2 /MT /LD /source-charset:utf-8 /execution-charset:utf-8 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DDUXUN_ENABLE_CUDA_RENDER=1 ^
  /I"%OFX_INC%" ^
  /Fo"%OUT%\DuXunFilmSim.obj" ^
  /Fe"%OUT%\DuXunFilmSim.ofx" ^
  "%SRC%\DuXunFilmSim.cpp" ^
  /link /DEF:"%SRC%\DuXunFilmSim.def" /SUBSYSTEM:CONSOLE ^
  > "%OUT%\compile_log.txt" 2>&1

set "CL_EXIT=%ERRORLEVEL%"
echo EXIT_CODE=%CL_EXIT% >> "%OUT%\compile_log.txt"

if "%CL_EXIT%"=="0" if exist "%OUT%\DuXunFilmSim.ofx" (
    echo SUCCESS: DuXunFilmSim.ofx created
    echo SUCCESS >> "%OUT%\compile_log.txt"
) else (
    echo FAILED: DuXunFilmSim.ofx not created
    echo FAILED >> "%OUT%\compile_log.txt"
    exit /b 1
)

if not "%CL_EXIT%"=="0" (
    echo FAILED: compiler returned %CL_EXIT%
    echo FAILED >> "%OUT%\compile_log.txt"
    exit /b %CL_EXIT%
)

goto :eof

:fail
echo VCVARSALL FAILED
echo VCVARSALL_FAILED > "%OUT%\compile_log.txt"
