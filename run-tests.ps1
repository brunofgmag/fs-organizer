param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Debug',

    [string]$Filter = '',

    [switch]$Reconfigure
)

$ErrorActionPreference = 'Stop'

if ($Filter -and ($Filter -like '*\*' -or $Filter -like '*/*' -or $Filter -like '*.cpp')) {
    $fileName = Split-Path -Leaf $Filter
    Write-Host "==> Caminho de arquivo detectado no filtro: $fileName"

    if ($fileName -like 'tst_*.cpp') {
        $Filter = $fileName -replace '^tst_', '' -replace '\.cpp$', '' -replace '_', '-'
        Write-Host "==> Mapeado para o teste CTest: $Filter"
    }
    else {
        Write-Host '==> Arquivo não reconhecido como teste. Rodando todos.'
        $Filter = ''
    }
}

$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCmd) {
    $cmake = $cmakeCmd.Source
}
else {
    $candidates = @(
        "$env:ProgramFiles\CMake\bin\cmake.exe",
        "$env:LOCALAPPDATA\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe"
    )
    $cmake = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $cmake) { throw 'cmake.exe não encontrado. Instale o CMake ou o CLion.' }

$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest)) { throw 'ctest.exe não encontrado ao lado do cmake.' }

if (-not $env:QT_ROOT_DIR) {
    $kit = Get-ChildItem -LiteralPath 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
        ForEach-Object {
            $p = Join-Path $_.FullName 'msvc2022_64'
            if (Test-Path "$p\bin\windeployqt.exe") { Get-Item $p }
        } |
        Sort-Object { [version]$_.Parent.Name } -Descending |
        Select-Object -First 1
    if ($kit) { $env:QT_ROOT_DIR = $kit.FullName }
}

$preset = $Config.ToLowerInvariant()
$buildDir = Join-Path $PSScriptRoot "build/$preset"

$cacheFile = Join-Path $buildDir 'CMakeCache.txt'
if ($Reconfigure -or -not (Test-Path -LiteralPath $cacheFile)) {
    Write-Host "==> Configurando preset '$preset'..."
    & $cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Filter) {
    $targetToBuild = "fsorg-$Filter-tests"
    Write-Host "==> Compilando apenas o alvo de teste: $targetToBuild ($Config)..."
    & $cmake --build --preset $preset --target $targetToBuild --parallel
}
else {
    Write-Host "==> Compilando todos os alvos ($Config)..."
    & $cmake --build --preset $preset --parallel
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n==> Rodando testes..."
$ctestArgs = @(
    '--test-dir', $buildDir
    '-C', $Config
    '--output-on-failure'
    '-j', [Environment]::ProcessorCount
)
if ($Filter) { $ctestArgs += '-R', $Filter }

& $ctest @ctestArgs
exit $LASTEXITCODE
