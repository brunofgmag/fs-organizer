param(
    [Parameter(Mandatory)]
    [string]$DeploymentDir,

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',

    [double]$MaximumSizeMiB = 0,

    [int]$MaximumFileCount = 80
)

$ErrorActionPreference = 'Stop'

$root = [IO.Path]::GetFullPath($DeploymentDir)
$isDebug = $Configuration -eq 'Debug'
$suffix = if ($isDebug) { 'd' } else { '' }
if ($MaximumSizeMiB -le 0) {
    $MaximumSizeMiB = if ($isDebug) { 250 } else { 60 }
}

$requiredPaths = @(
    'fs-organizer.exe',
    'Archivo-OFL.txt',
    "Qt6Core$suffix.dll",
    "Qt6Gui$suffix.dll",
    "Qt6Widgets$suffix.dll",
    "Qt6Network$suffix.dll",
    "platforms/qwindows$suffix.dll",
    "styles/qmodernwindowsstyle$suffix.dll",
    "tls/qschannelbackend$suffix.dll",
    'translations/qt_pt_BR.qm'
)
if (-not $isDebug) {
    $requiredPaths += @(
        'msvcp140.dll',
        'vcruntime140.dll',
        'vcruntime140_1.dll'
    )
}

$forbiddenPaths = @(
    'D3Dcompiler_47.dll',
    'dxcompiler.dll',
    'dxil.dll',
    'opengl32sw.dll',
    'vc_redist.x64.exe',
    'Qt6Pdf.dll',
    'Qt6Pdfd.dll',
    'Qt6Svg.dll',
    'Qt6Svgd.dll',
    'Qt6VirtualKeyboard.dll',
    'Qt6VirtualKeyboardd.dll',
    'generic',
    'iconengines',
    'imageformats',
    'networkinformation',
    'platforminputcontexts',
    'qmltooling',
    'translations/qt_de.qm'
)

$missing = $requiredPaths | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $root $_))
}
if ($missing) {
    throw "Deployment is missing required paths:`n$($missing -join "`n")"
}

$unexpected = $forbiddenPaths | Where-Object {
    Test-Path -LiteralPath (Join-Path $root $_)
}
if ($unexpected) {
    throw "Deployment contains unused paths:`n$($unexpected -join "`n")"
}

$files = @(Get-ChildItem -LiteralPath $root -File -Recurse)
$totalBytes = ($files | Measure-Object Length -Sum).Sum
$totalMiB = [math]::Round($totalBytes / 1MB, 2)

if ($totalMiB -gt $MaximumSizeMiB) {
    throw "Deployment is $totalMiB MiB; maximum allowed is $MaximumSizeMiB MiB."
}
if ($files.Count -gt $MaximumFileCount) {
    throw "Deployment contains $($files.Count) files; maximum allowed is $MaximumFileCount."
}

$exeSize = (Get-Item -LiteralPath (Join-Path $root 'fs-organizer.exe')).Length
if ($exeSize -lt 100KB) {
    throw "Executable is suspiciously small: $exeSize bytes."
}

Write-Host "Deployment verified: $($files.Count) files, $totalMiB MiB."
