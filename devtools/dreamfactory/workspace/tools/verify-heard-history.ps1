# SPDX-License-Identifier: GPL-3.0-or-later
# Verify real spoken-line learning survives restoring an earlier earned save.
param([string]$RestoreCheckpoint='build/save-dust-blood-hotel.dfsv')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/heard-real/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    & tools/trace-boot.ps1 -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$run/trade.dfsv" -SavePath "$run/saves" -InventoryCompact -GuiKeys @('pad:up','pad:a') -Choices @('fear.pup:101') -SkipSpeech -Actions @('dumpheard','openinventory','expectglobal handitem ring','padon actor fear','expectowner ring fear','expectglobal phase 5','dumpheard') *> "$run/trade.log"
    Copy-Item build/boot-dust.log "$run/trade-native.log"
    $counts = [regex]::Matches((Get-Content "$run/trade-native.log" -Raw), 'BOOT HEARD lines=(\d+)')
    if ($counts.Count -ne 2 -or [int]$counts[1].Groups[1].Value -le [int]$counts[0].Groups[1].Value) {throw 'Native spoken lines were not learned'}
    & tools/trace-boot.ps1 -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$run/restart.dfsv" -SavePath "$run/saves" -Actions @('expectowner ring stranger','dumpheard') *> "$run/restart.log"
    Copy-Item build/boot-dust.log "$run/restart-native.log"
    $restart = [regex]::Match((Get-Content "$run/restart-native.log" -Raw), 'BOOT HEARD lines=(\d+)')
    if ($restart.Groups[1].Value -ne $counts[1].Groups[1].Value) {throw 'Heard history lost or duplicated during rewind/restart'}
    @{passed=$true; linesBefore=[int]$counts[0].Groups[1].Value; linesAfter=[int]$counts[1].Groups[1].Value; scope='Real Dust speech learns exact lines; fresh process restoring a pre-trade native save retains heard history while reverting item ownership. Replay controls are verified separately.'; binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Native heard history PASS: $run"
} finally {Pop-Location}
