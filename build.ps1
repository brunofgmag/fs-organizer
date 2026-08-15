param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Release',
    [switch]$RunTests,
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'

if ($RunTests -and $SkipTests) {
    throw 'Use either -RunTests or -SkipTests, not both.'
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
}
else {
    $cmakeCandidates = @(
        (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\CLion\bin\cmake\win\x64\bin\cmake.exe')
    )

    $cmake = $cmakeCandidates |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
}

if (-not $cmake) {
    throw 'CMake was not found. Install CMake or CLion and ensure cmake.exe is available.'
}

$preset = $Config.ToLowerInvariant()
$buildDir = Join-Path $PSScriptRoot "build/$preset"
$exe = Join-Path $buildDir 'bin/fs-organizer.exe'
$cacheFile = Join-Path $buildDir 'CMakeCache.txt'

if (-not $env:QT_ROOT_DIR) {
    $qtRoot = 'C:\Qt'
    if (Test-Path -LiteralPath $qtRoot) {
        $kit = Get-ChildItem -LiteralPath $qtRoot -Directory |
            ForEach-Object {
                $candidate = Join-Path $_.FullName 'msvc2022_64'
                if (Test-Path -LiteralPath (Join-Path $candidate 'bin\windeployqt.exe')) {
                    Get-Item -LiteralPath $candidate
                }
            } |
            Sort-Object { [version]$_.Parent.Name } -Descending |
            Select-Object -First 1

        if ($kit) {
            $env:QT_ROOT_DIR = $kit.FullName
            Write-Host "Using Qt kit: $($env:QT_ROOT_DIR)"
        }
    }
}

if (-not $env:QT_ROOT_DIR -or
    -not (Test-Path -LiteralPath (Join-Path $env:QT_ROOT_DIR 'bin\windeployqt.exe'))) {
    throw 'QT_ROOT_DIR must point to a Qt 6.8+ msvc2022_64 kit.'
}

if (Test-Path -LiteralPath $cacheFile) {
    $cacheLines = Get-Content -LiteralPath $cacheFile
    $usesVisualStudio =
        $cacheLines -contains 'CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022'
    $usesX64 =
        $cacheLines -contains 'CMAKE_GENERATOR_PLATFORM:INTERNAL=x64'

    if (-not $usesVisualStudio -or -not $usesX64) {
        Write-Host 'Removing incompatible CMake cache before configuring MSVC x64.'
        Remove-Item -LiteralPath $cacheFile -Force

        $cmakeFiles = Join-Path $buildDir 'CMakeFiles'
        if (Test-Path -LiteralPath $cmakeFiles) {
            Remove-Item -LiteralPath $cmakeFiles -Recurse -Force
        }
    }
}

$configureArgs = @('--preset', $preset)
if ($SkipTests) { $configureArgs += '-DBUILD_TESTING=OFF' }
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build --preset $preset --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($RunTests) {
    $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
    if (-not (Test-Path -LiteralPath $ctest)) {
        throw 'CTest was not found next to the selected CMake executable.'
    }

    & $ctest --test-dir $buildDir -C $Config --output-on-failure -j ([Environment]::ProcessorCount)
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& (Join-Path $PSScriptRoot 'tools/verify-deployment.ps1') `
    -DeploymentDir (Join-Path $buildDir 'bin') `
    -Configuration $Config
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$registry = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'cmake') -Filter '*.cmake' -File)
$registry += Get-Item -LiteralPath (Join-Path $PSScriptRoot 'CMakeLists.txt')
$newestRegistry = ($registry | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).LastWriteTimeUtc

$justConfigured = [System.IO.Path]::GetFullPath($buildDir)

$stale = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'build') -Directory |
    Where-Object { $_.FullName -ne $justConfigured } |
    Where-Object {
        $generated = Join-Path $_.FullName 'CMakeFiles'
        (Test-Path -LiteralPath (Join-Path $_.FullName 'CMakeCache.txt')) -and
        (Test-Path -LiteralPath $generated) -and
        ((Get-Item -LiteralPath $generated).LastWriteTimeUtc -lt $newestRegistry)
    }

if ($stale) {
    Write-Warning 'These build directories were configured before the current cmake/ registry:'
    $stale | ForEach-Object { Write-Warning "  $($_.Name)" }
    Write-Warning 'They cannot see files added since, and "cmake --build" alone will not reconfigure them.'
    Write-Warning 'Run "cmake -S . -B build/<name>" on each before building it, or the link fails with LNK2019.'
}

Write-Host "Executable build ready: $exe"
