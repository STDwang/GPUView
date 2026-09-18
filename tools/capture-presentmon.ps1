param([string]$PresentMon = '', [int]$Seconds = 8)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if ($Seconds -lt 1 -or $Seconds -gt 15) { throw 'Seconds must be between 1 and 15 (target exits at 20 seconds).' }
if (-not $PresentMon) { $PresentMon = Join-Path $projectRoot '.tools/PresentMon/PresentMon-1.9.2-x64.exe' }
if (-not (Test-Path -LiteralPath $PresentMon)) { throw 'Download PresentMon 1.9.2 x64 from its official GitHub release first.' }
$hash = (Get-FileHash -LiteralPath $PresentMon -Algorithm SHA256).Hash
if ($hash -ne 'EE4EBA300A521B16291B7ACE14D95EFB9FA149B0EF1D31B15DF4EE345C4E002B') { throw 'This capture script is verified only for the pinned PresentMon 1.9.2 x64 binary.' }
$targetExe = Join-Path $projectRoot 'build/Release/gpuview_capture_target.exe'
if (-not (Test-Path -LiteralPath $targetExe)) { throw 'Build Release first.' }
$rawDir = Join-Path $projectRoot 'data/raw'
New-Item -ItemType Directory -Force -Path $rawDir | Out-Null
$captureName = 'presentmon-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
$outputFile = Join-Path $rawDir ($captureName + '.csv')
# 只采集本项目新启动的目标PID，不采集用户的其他应用；目标自动退出。
$target = Start-Process -FilePath $targetExe -WindowStyle Hidden -PassThru
& $PresentMon -process_id $target.Id -output_file $outputFile -timed $Seconds -terminate_after_timed -no_top -session_name $captureName
if ($LASTEXITCODE -ne 0) { throw 'PresentMon capture failed; inspect its output.' }
@{ toolVersion='1.9.2'; toolSha256=$hash; target='gpuview_capture_target.exe'; targetPid=$target.Id; durationSeconds=$Seconds; capturedAt=(Get-Date).ToString('o'); rawSha256=(Get-FileHash -LiteralPath $outputFile).Hash; synthetic=$false; workload='D3D11 Clear + Present, 70ms sleep every 60 frames' } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $rawDir ($captureName + '.json')) -Encoding UTF8
Write-Output $outputFile
