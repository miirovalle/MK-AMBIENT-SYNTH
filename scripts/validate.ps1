<#
    Full validation pass for the portable engine:
      1. build core + tests + renderer
      2. run the DSP test suite
      3. render every preset/probe to WAV
      4. analyse spectra, stereo width, decay and aliasing with numpy

    Usage:  ./scripts/validate.ps1 [-Configuration Release] [-SkipAnalysis]
#>
param(
    [string] $Configuration = "Release",
    [switch] $SkipAnalysis
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build-cmake"
$renderDir = Join-Path $buildDir "renders"

Write-Host "== configure ==" -ForegroundColor Cyan
cmake -S $root -B $buildDir -G "Visual Studio 18 2026" -A x64 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

Write-Host "== build ==" -ForegroundColor Cyan
cmake --build $buildDir --config $Configuration --target mk_tests mk_render | Out-Null
if ($LASTEXITCODE -ne 0) { throw "build failed" }

Write-Host "== dsp tests ==" -ForegroundColor Cyan
& (Join-Path $buildDir "$Configuration\mk_tests.exe")
if ($LASTEXITCODE -ne 0) { throw "DSP tests failed" }

Write-Host "== render ==" -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $renderDir | Out-Null
& (Join-Path $buildDir "$Configuration\mk_render.exe") $renderDir
if ($LASTEXITCODE -ne 0) { throw "render failed" }

if (-not $SkipAnalysis) {
    Write-Host "== analysis ==" -ForegroundColor Cyan
    python (Join-Path $root "Tools\analyze_renders.py") $renderDir
    if ($LASTEXITCODE -ne 0) { throw "audio analysis failed" }
}

Write-Host "All checks passed." -ForegroundColor Green
