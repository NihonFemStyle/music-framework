[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string]$Architecture = 'x64',

    [switch]$NoCRT,
    [switch]$SDL,
    [switch]$CLion,

    [ValidateSet('MSBuild', 'MinGW')]
    [string]$CLionToolchain = 'MSBuild',

    [switch]$Clean,

    [ValidateRange(1, 128)]
    [int]$Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildSuffix = if ($NoCRT) { 'nocrt' } elseif ($SDL) { 'sdl' } else { 'default' }
$buildDirectoryName = if ($CLion -and $CLionToolchain -eq 'MinGW') { "cmake-build-clion-mingw-$buildSuffix" } elseif ($CLion) { "cmake-build-clion-$Architecture-$buildSuffix" } else { "build-$Architecture-$buildSuffix" }
$buildDirectory = Join-Path $projectRoot $buildDirectoryName

if ($CLion -and $CLionToolchain -eq 'MinGW' -and $NoCRT) {
    throw 'The constrained no-CRT build requires MSVC. Use -CLionToolchain MSBuild with -NoCRT.'
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake was not found in PATH. Install CMake or add it to PATH and try again.'
}

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    Write-Host "Removing $buildDirectory"
    Remove-Item -LiteralPath $buildDirectory -Recurse -Force
}

$configureArguments = if ($CLion) {
    $presetPrefix = if ($CLionToolchain -eq 'MinGW') { 'clion-mingw' } else { "clion-$($Architecture.ToLowerInvariant())" }
    @('--preset', "$presetPrefix-$buildSuffix")
} else {
    @(
        '-S', $projectRoot,
        '-B', $buildDirectory,
        '-G', 'Visual Studio 17 2022',
        '-A', $Architecture,
        "-DMUSICOVERLAY_NOCRT=$($NoCRT.IsPresent.ToString().ToUpperInvariant())"
        "-DMUSICOVERLAY_BUILD_SDL_FRONTEND=$($SDL.IsPresent.ToString().ToUpperInvariant())"
    )
}

if ($NoCRT -and -not $CLion) {
    # The constrained no-CRT executable intentionally does not link the C++ integration library.
    $configureArguments += '-DMUSICOVERLAY_BUILD_INTEGRATION=OFF'
}

Write-Host "Configuring MusicOverlay ($Architecture, $Configuration, NOCRT=$($NoCRT.IsPresent), SDL=$($SDL.IsPresent), CLion=$($CLion.IsPresent), CLionToolchain=$CLionToolchain)"
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
if ($SDL) {
    $sdlExecutable = Join-Path $buildDirectory "$Configuration\musicoverlay_sdl.exe"
    if (Test-Path -LiteralPath $sdlExecutable) {
        Write-Host "SDL build complete: $sdlExecutable" -ForegroundColor Green
    }
}
