@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
mkdir "C:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin\build" 2>nul
cd /d "C:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin"
cl.exe /nologo /EHsc /O2 /MD /c /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS ^
  /I"openfx-sdk\include" ^
  /I"openfx-sdk\Support\include" ^
  /I"ofx\DuXunFilm" ^
  /I"presets\film_stocks" ^
  /Fo"build\\" ^
  "ofx\DuXunFilm\DuXunPlugin.cpp" > build\compile_log.txt 2>&1
echo RESULT=%ERRORLEVEL% >> build\compile_log.txt
