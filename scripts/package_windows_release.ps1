param(
    [string]$Version = "v5.0-free",
    [string]$OutputDir = "dist",
    [string]$BuildOfx = "build\DuXunFilmSim.ofx"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..")
$BuildOfxPath = Resolve-Path (Join-Path $ProjectRoot $BuildOfx)
$PackagingDir = Join-Path $ProjectRoot "packaging\windows"
$OutputRoot = Join-Path $ProjectRoot $OutputDir
$PackageName = "DuXunFilmSimulation-$Version-windows"
$StageDir = Join-Path $OutputRoot $PackageName
$BundleDir = Join-Path $StageDir "DuXunFilmSim.ofx.bundle"
$Win64Dir = Join-Path $BundleDir "Contents\Win64"
$ZipPath = Join-Path $OutputRoot "$PackageName.zip"
$ZipHashPath = "$ZipPath.sha256"

if (!(Test-Path (Join-Path $PackagingDir "install.bat"))) {
    throw "Missing packaging template folder: $PackagingDir"
}

if (!(Test-Path $OutputRoot)) {
    New-Item -ItemType Directory -Path $OutputRoot | Out-Null
}

if (Test-Path $StageDir) {
    Remove-Item -LiteralPath $StageDir -Recurse -Force
}

if (Test-Path $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}

if (Test-Path $ZipHashPath) {
    Remove-Item -LiteralPath $ZipHashPath -Force
}

New-Item -ItemType Directory -Path $Win64Dir | Out-Null

Copy-Item -LiteralPath (Join-Path $PackagingDir "install.bat") -Destination (Join-Path $StageDir "Install.bat")
Copy-Item -LiteralPath (Join-Path $PackagingDir "uninstall.bat") -Destination (Join-Path $StageDir "Uninstall.bat")
Copy-Item -LiteralPath (Join-Path $PackagingDir "README.zh-CN.md") -Destination (Join-Path $StageDir "README.zh-CN.md")
Copy-Item -LiteralPath (Join-Path $PackagingDir "README.en.md") -Destination (Join-Path $StageDir "README.en.md")
Copy-Item -LiteralPath (Join-Path $ProjectRoot "LICENSE") -Destination (Join-Path $StageDir "LICENSE.txt")
Copy-Item -LiteralPath $BuildOfxPath -Destination (Join-Path $Win64Dir "DuXunFilmSim.ofx")

$PluginHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $Win64Dir "DuXunFilmSim.ofx")).Hash
$ChecksumText = @"
SHA256 checksums

DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx  $PluginHash
"@
Set-Content -LiteralPath (Join-Path $StageDir "CHECKSUMS-SHA256.txt") -Value $ChecksumText -Encoding UTF8

Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $ZipPath -CompressionLevel Optimal

$ZipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ZipPath).Hash
Set-Content -LiteralPath $ZipHashPath -Value "$ZipHash  $(Split-Path -Leaf $ZipPath)" -Encoding ASCII

Write-Host "Package: $ZipPath"
Write-Host "Package SHA256: $ZipHash"
Write-Host "Staging directory: $StageDir"
