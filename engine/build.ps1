# Build script for the C++ engine.
#
# Why this exists: winget put g++ on the *registry* PATH, but already-open shells don't
# see it until refreshed. This script refreshes PATH from the registry every time, so the
# build works reliably without you having to restart anything.
#
# Usage:   .\engine\build.ps1            (builds optimized engine.exe)
#          .\engine\build.ps1 -Run       (build, then run it)

param([switch]$Run, [string]$OutName = "engine.exe")

$ErrorActionPreference = "Stop"

# Refresh PATH from registry (Machine + User) so g++ is always found.
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
            [System.Environment]::GetEnvironmentVariable("Path", "User")

$here    = Split-Path -Parent $MyInvocation.MyCommand.Path
$src     = Join-Path $here "src"
$build   = Join-Path $here "build"
$out     = Join-Path $build $OutName
New-Item -ItemType Directory -Force -Path $build | Out-Null

# Gather every .cpp under src/. The @(...) forces an array even when there's exactly one
# file — otherwise PowerShell would splat a lone string character-by-character to g++.
$sources = @(Get-ChildItem -Path $src -Filter *.cpp -Recurse | ForEach-Object { $_.FullName })

# Flags: C++20, aggressive optimization, tuned for THIS machine's CPU (-march=native),
# all warnings on. -static bakes the C++/gcc/pthread runtimes INTO the exe so it runs even
# when the MinGW DLL folder isn't on PATH (e.g. when python-chess launches it as a subprocess).
$flags = @("-std=c++20", "-O2", "-march=native", "-Wall", "-Wextra", "-static")

Write-Host "Compiling $($sources.Count) file(s) -> $out"
& g++ @flags @sources -o $out
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }
Write-Host "Build OK: $out" -ForegroundColor Green

if ($Run) {
    Write-Host "--- running ---"
    & $out
}
