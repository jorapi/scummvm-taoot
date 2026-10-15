# SPDX-License-Identifier: GPL-3.0-or-later
# Cut and restore A14's power using the authored fuse panel and Sasha movement.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-charles-necklace.dfsv", [int]$TimeoutSeconds = 600)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-FuseRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-fuse-panel-ready.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal neckphase 6', 'clickon prop map', 'key a', 'expectflat Map 2',
    'clickon button Button34', 'expectset gstair2', 'expectview View37'
) + @(Get-FuseRoute GSTAIR2 View37 View22) + @(
    'clickon painting door', 'key uparrow', 'expectset halla', 'expectview View95', 'expectglobal hallside port'
) + @(Get-FuseRoute HALLA View95 View61) + @(
    'idle 120', 'clickon painting fuse', 'expectflat main 1',
    'clickon painting fuse', 'expectflat fuse 1', 'clickon prop fusedoor', 'expectpropview fusedoor open',
    'dumpprop fuse14', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-fuse-panel-ready-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-fuse-panel-ready.dfsv" -Checkpoint "$root/build/save-full-sasha-poweroff.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectflat fuse 1', 'clickon button fuse14', 'waitpropview fuse14 off',
    'click 450 355', 'expectflat main 1', 'expectglobal neckphase 7', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-sasha-poweroff-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-sasha-poweroff.dfsv" -Checkpoint "$root/build/save-full-sasha-poweron.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectset halla', 'expectview View61', 'expectglobal neckphase 7',
    'idle 120', 'clickon painting fuse', 'expectflat fuse 1', 'clickon prop fusedoor', 'expectpropview fusedoor open',
    'clickon prop fuse14', 'waitpropview fuse14 light', 'click 450 355',
    'expectflat main 1', 'expectglobal neckphase 8', 'expectowner fakeneck frank', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-sasha-poweron-native.log"
