# Requires PowerShell 5+
param(
    [string]$Version = "v3.10.0"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$dstDir = Join-Path $root "vendor\Catch2\extras"
New-Item -ItemType Directory -Force -Path $dstDir | Out-Null

$hppUrl = "https://raw.githubusercontent.com/catchorg/Catch2/$Version/extras/catch_amalgamated.hpp"
$cppUrl = "https://raw.githubusercontent.com/catchorg/Catch2/$Version/extras/catch_amalgamated.cpp"

Invoke-WebRequest -Uri $hppUrl -OutFile (Join-Path $dstDir "catch_amalgamated.hpp")
Invoke-WebRequest -Uri $cppUrl -OutFile (Join-Path $dstDir "catch_amalgamated.cpp")

Write-Host "Downloaded Catch2 amalgamated files for $Version to $dstDir"
