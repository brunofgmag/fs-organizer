param(
    [Parameter(Mandatory)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'

$path = [IO.Path]::GetFullPath($Executable)
if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    throw "There is no executable at $path."
}

$latin1 = [Text.Encoding]::GetEncoding(28591)
$asLatin1 = $latin1.GetString([IO.File]::ReadAllBytes($path))

$telltales = @('apply.ps1', 'Expand-Archive', 'releases/download', 'robocopy')

$found = foreach ($telltale in $telltales) {
    $encodings = [ordered]@{
        'ASCII' = [Text.Encoding]::ASCII
        'UTF-16LE' = [Text.Encoding]::Unicode
    }

    foreach ($named in $encodings.Keys) {
        $needle = $latin1.GetString($encodings[$named].GetBytes($telltale))
        $at = $asLatin1.IndexOf($needle, [StringComparison]::OrdinalIgnoreCase)

        if ($at -ge 0) {
            "  '$telltale' in $named at offset $at"
        }
    }
}

if ($found) {
    Write-Host "The executable still carries the self-updater: $path"
    $found | ForEach-Object { Write-Host $_ }
    exit 1
}

Write-Host "No trace of the self-updater in $path."
exit 0
