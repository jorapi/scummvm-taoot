# SPDX-License-Identifier: GPL-3.0-or-later
# Charles explains Sasha's necklace hiding place in the smoking room.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-georgia-b70.dfsv", [int]$TimeoutSeconds = 600)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-CharlesRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-charles-necklace.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('charl1.pup:100','charl1.pup:101','charl1.pup:102','charl1.pup:999') -Actions (@(
    'expectglobal neckphase 5', 'expectowner fakeneck frank', 'clickon prop map',
    'key a', 'expectflat Map 2', 'clickon button Button32', 'expectset stair1c1', 'expectview View74'
) + @(Get-CharlesRoute STAIR1C1 View74 View94) + @(
    'clickon painting door', 'key uparrow', 'expectset smoke', 'expectview View33'
) + @(Get-CharlesRoute SMOKE View33 View68) + @(
    'clickon actor charl', 'expectglobal neckphase 6', 'expectowner fakeneck frank', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-charles-necklace-native.log"
