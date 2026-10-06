[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string]$Architecture = 'x64',

    [switch]$NoCRT,
    [switch]$Clean,

    [ValidateRange(1, 128)]
    [int]$Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildSuffix = if ($NoCRT) { 'nocrt' } else { 'default' }
$buildDirectory = Join-Path $projectRoot "build-$Architecture-$buildSuffix"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake was not found in PATH. Install CMake or add it to PATH and try again.'
}

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    Write-Host "Removing $buildDirectory"
    Remove-Item -LiteralPath $buildDirectory -Recurse -Force
}

$configureArguments = @(
    '-S', $projectRoot,
    '-B', $buildDirectory,
    '-G', 'Visual Studio 17 2022',
    '-A', $Architecture,
    "-DMUSICOVERLAY_NOCRT=$($NoCRT.IsPresent.ToString().ToUpperInvariant())"
)

if ($NoCRT) {
    # The constrained no-CRT executable intentionally does not link the C++ integration library.
    $configureArguments += '-DMUSICOVERLAY_BUILD_INTEGRATION=OFF'
}

Write-Host "Configuring MusicOverlay ($Architecture, $Configuration, NOCRT=$($NoCRT.IsPresent))"
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

Write-Host "Building MusicOverlay with $Jobs parallel jobs"
& cmake --build $buildDirectory --config $Configuration --parallel $Jobs
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

$executable = Join-Path $buildDirectory "$Configuration\musicoverlay_app.exe"
if (Test-Path -LiteralPath $executable) {
    Write-Host "Build complete: $executable" -ForegroundColor Green
} else {
    Write-Host "Build completed, but the executable was not found at the expected path: $executable" -ForegroundColor Yellow
}
