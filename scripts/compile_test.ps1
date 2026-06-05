$ErrorActionPreference = 'Stop'
$vsPath = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
$workDir = 'C:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin'
$buildDir = "$workDir\build"
$ofxInc = "$workDir\openfx-sdk\include"
$ofxSupport = "$workDir\openfx-sdk\Support\include"
$srcDir = "$workDir\ofx\DuXunFilm"
$presetDir = "$workDir\presets\film_stocks"

if (-not (Test-Path $vsPath)) {
    Write-Error "vcvarsall.bat not found"
    exit 1
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$env:INCLUDE = "$ofxInc;$ofxSupport;$srcDir;$presetDir"
$env:LIB = ""

# Source vcvarsall via cmd.exe and capture env
$vcVarsOutput = cmd.exe /C "`"$vsPath`" x64 >nul 2>&1 && set" 2>&1 | Out-String
# Extract INCLUDE and LIB from vcvars output
foreach ($line in $vcVarsOutput -split "`n") {
    if ($line -match '^INCLUDE=(.*)$') { $env:INCLUDE = "$($Matches[1]);" + $env:INCLUDE }
    if ($line -match '^LIB=(.*)$') { $env:LIB = $Matches[1] }
    if ($line -match '^PATH=(.*)$') { $env:PATH = $Matches[1] + ";" + $env:PATH }
}

$clArgs = @(
    '/nologo', '/EHsc', '/O2', '/MD', '/c',
    '/DNOMINMAX', '/DWIN32_LEAN_AND_MEAN', '/D_CRT_SECURE_NO_WARNINGS',
    "/I`"$ofxInc`"",
    "/I`"$ofxSupport`"",
    "/I`"$srcDir`"",
    "/I`"$presetDir`"",
    "/Fo`"$buildDir\\`"",
    "`"$srcDir\DuXunPlugin.cpp`""
)

try {
    $result = & cmd.exe /C "cl.exe $clArgs" 2>&1 | Out-String
    $result | Out-File -FilePath "$buildDir\compile_log.txt" -Encoding utf8
    Write-Output $result
} catch {
    Write-Error "Compile failed: $_"
    exit 1
}
