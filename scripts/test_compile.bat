@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
mkdir "C:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin\build" 2>nul
cl.exe /nologo /EHsc /O2 /MD /c /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DCPPWIN /EHsc /wd4577 /wd4996 ^
  /IC:\Users\LI\WorkBuddy\2026-05-28-19-53-09\openfx-sdk\include ^
  /IC:\Users\LI\WorkBuddy\2026-05-28-19-53-09\openfx-sdk\Support\include ^
  /IC:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin\ofx\DuXunFilm ^
  /IC:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin\presets\film_stocks ^
  /Fo"C:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin\build\\" ^
  "C:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin\ofx\DuXunFilm\DuXunPlugin.cpp" ^
  2>&1
echo EXIT_CODE=%ERRORLEVEL%
