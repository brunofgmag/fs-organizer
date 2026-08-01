param(
    [Parameter(Mandatory)]
    [string]$DeploymentDir
)

$ErrorActionPreference = 'Stop'

$root = [IO.Path]::GetFullPath($DeploymentDir)
$exe = Join-Path $root 'fs-organizer.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Refusing to prune a directory without fs-organizer.exe: $root"
}

$unusedPaths = @(
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
    'qmltooling'
)

foreach ($relativePath in $unusedPaths) {
    $path = Join-Path $root $relativePath
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

Get-ChildItem -LiteralPath $root -Filter 'qcertonlybackend*.dll' -File -Recurse |
    Remove-Item -Force

$spokenHere = @('pt_BR', 'en')
$translations = Join-Path $root 'translations'
if (Test-Path -LiteralPath $translations) {
    Get-ChildItem -LiteralPath $translations -Filter '*.qm' -File |
        Where-Object {
            $language = $_.BaseName -replace '^[^_]+_', ''
            $spokenHere -notcontains $language
        } |
        Remove-Item -Force
}

Get-ChildItem -LiteralPath $root -Directory -Recurse |
    Sort-Object { $_.FullName.Length } -Descending |
    Where-Object { -not (Get-ChildItem -LiteralPath $_.FullName -Force) } |
    Remove-Item -Force
