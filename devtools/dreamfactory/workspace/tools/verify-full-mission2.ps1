# SPDX-License-Identifier: GPL-3.0-or-later
# Finish the painting/ring mission through authored Claris and Penny dialogue.
# Requires the ring checkpoint from verify-full-fencing.ps1; no story-state injection.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-ring.dfsv", [int]$TimeoutSeconds = 480)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-Mission2Route([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-claris.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 2', 'expectglobal phase 2', 'expectowner ring frank'
) + @(Get-Mission2Route RECEPT1C View25 View91) + @(
    'clickon painting door', 'key uparrow', 'expectset halld', 'expectview View31'
) + @(Get-Mission2Route HALLD View31 View79) + @(
    'clickon painting knock', 'expectglobal mission 2', 'expectglobal phase 3',
    'expectowner ring frank', 'expectowner painting frank', 'savecheckpoint'
)) -Choices @('claris1.pup:101', 'claris1.pup:101', 'claris1.pup:101', 'claris1.pup:101')
Copy-Item "$root/build/boot-full.log" "$root/build/full-claris-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-claris.dfsv" -Checkpoint "$root/build/save-full-mission3.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 2', 'expectglobal phase 3',
    'clickon prop map', 'key f', 'expectflat Map 7', 'clickon button Button22',
    'expectset stair2c', 'expectview View28', 'clickon painting door',
    'key uparrow', 'expectset hallf2c', 'expectview View70'
) + @(Get-Mission2Route HALLF2C View70 View76) + @(
    'clickon painting knock', 'expectglobal mission 3', 'expectglobal phase 0',
    'expectflat main 1', 'expectowner gaspen frank', 'expectowner painting frank',
    'expectowner ring frank', 'expectglobal playerdeath ', 'savecheckpoint'
)) -Choices @('penny1.pup:202', 'penny1.pup:102', 'penny1.pup:101', 'penny1.pup:500', 'penny1.pup:600') -ClickMovies @('plans.mov', 'penybook.mov')
Copy-Item "$root/build/boot-full.log" "$root/build/full-mission3-native.log"
