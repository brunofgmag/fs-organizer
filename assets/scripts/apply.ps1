param([int]$AppPid, [string]$Source, [string]$Dest, [string]$ExeName, [int]$Relaunch)
$root = Split-Path -Parent $PSCommandPath
Start-Transcript -Path (Join-Path $root 'apply.log') -Append | Out-Null
Wait-Process -Id $AppPid -Timeout 60 -ErrorAction SilentlyContinue
if (-not (Test-Path (Join-Path $Dest $ExeName))) { Stop-Transcript | Out-Null; exit 1 }
robocopy $Source $Dest /MIR /R:20 /W:1
if ($LASTEXITCODE -ge 8) { Stop-Transcript | Out-Null; exit 1 }
if ($Relaunch -eq 1) { Start-Process -FilePath (Join-Path $Dest $ExeName) -WorkingDirectory $Dest }
Stop-Transcript | Out-Null
Remove-Item -Recurse -Force (Join-Path $root 'download'), (Join-Path $root 'staged') -ErrorAction SilentlyContinue
Remove-Item -Force $PSCommandPath -ErrorAction SilentlyContinue
