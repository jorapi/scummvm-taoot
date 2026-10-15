# SPDX-License-Identifier: GPL-3.0-or-later
# Use Hacker's clue to recover Old Reds and obtain Max's smokestack lead.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-hacker-clue.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-CigaretteRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-oldreds.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 3', 'expectglobal phase 0', 'expectowner hackclue frank',
    'clickon prop map', 'key a', 'expectflat Map 2', 'clickon button Button32',
    'expectset stair1c1', 'expectview View74'
) + @(Get-CigaretteRoute STAIR1C1 View74 View40) + @(
    'clickon painting door', 'key uparrow', 'expectset cafe', 'expectview View59'
) + @(Get-CigaretteRoute CAFE View59 View44) + @(
    'clickon prop willzeittable', 'expectflat cuff 1',
    'clickon prop cigscigs', 'expectpropview cigscigs big', 'clickon prop cigscigs',
    'expectowner oldreds frank', 'expectowner hackclue xxxfrank',
    'click 457 351', 'expectflat main 1', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-oldreds-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-oldreds.dfsv" -Checkpoint "$root/build/save-full-smokestack-lead.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'clickon prop map', 'key a', 'expectflat Map 2', 'clickon button Button32',
    'expectset stair1c1', 'expectview View74'
) + @(Get-CigaretteRoute STAIR1C1 View74 View94) + @(
    'clickon painting door', 'key uparrow', 'expectset smoke', 'expectview View33'
) + @(
    'clickon actor max', 'expectowner oldreds max', 'expectglobal mission 3',
    'expectglobal phase 1', 'savecheckpoint'
)) -Choices @('max1.pup:102')
Copy-Item "$root/build/boot-full.log" "$root/build/full-smokestack-lead-native.log"
