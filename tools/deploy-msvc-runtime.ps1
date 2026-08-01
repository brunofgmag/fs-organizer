param(
    [Parameter(Mandatory)]
    [string]$DeploymentDir,

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

if ($Configuration -eq 'Debug') {
    Write-Host 'Skipping the MSVC Debug runtime, which cannot be redistributed.'
    exit 0
}

$root = [IO.Path]::GetFullPath($DeploymentDir)
$exe = Join-Path $root 'fs-organizer.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Deployment directory does not contain fs-organizer.exe: $root"
}

$crtDir = $null
if ($env:VCToolsRedistDir) {
    $candidate = Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT'
    if (Test-Path -LiteralPath $candidate -PathType Container) {
        $crtDir = $candidate
    }
}

if (-not $crtDir) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'vswhere.exe was not found and VCToolsRedistDir is not configured.'
    }

    $installPath = & $vswhere `
        -latest `
        -products '*' `
        -version '[17.0,18.0)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $installPath) {
        throw 'Visual Studio 2022 with the MSVC x64 tools was not found.'
    }

    $redistRoot = Join-Path $installPath 'VC/Redist/MSVC'
    $crtDir = Get-ChildItem -LiteralPath $redistRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.' } |
        Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Join-Path $_.FullName 'x64/Microsoft.VC143.CRT' } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
        Select-Object -First 1
}

if (-not $crtDir) {
    throw 'The Microsoft.VC143.CRT redistributable directory was not found.'
}

$runtimeFiles = @(Get-ChildItem -LiteralPath $crtDir -Filter '*.dll' -File)
if (-not $runtimeFiles) {
    throw "No redistributable runtime DLLs were found in $crtDir."
}

Copy-Item -LiteralPath $runtimeFiles.FullName -Destination $root -Force
Write-Host "Deployed $($runtimeFiles.Count) MSVC runtime DLLs from $crtDir."
