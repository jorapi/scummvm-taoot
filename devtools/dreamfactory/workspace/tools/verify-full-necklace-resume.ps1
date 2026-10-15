# SPDX-License-Identifier: GPL-3.0-or-later
# Return to mission two's existing test entry point with the necklace branch intact.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-necklace-recovered.dfsv", [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/room-route.mjs" full HALLF2C View70 View76
if ($LASTEXITCODE) { throw 'Cannot plan Penny corridor route' }
$route = @($route | ForEach-Object { ($_ -split ' # ')[0] })
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-necklace-mission2.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 2', 'expectglobal phase 0', 'expectglobal neckphase 9',
    'expectowner realneck frank', 'expectowner fakeneck vlad', 'clickon prop map', 'key f',
    'expectflat Map 7', 'clickon button Button22', 'expectset stair2c', 'expectview View28',
    'clickon painting door', 'key uparrow', 'expectset hallf2c', 'expectview View70'
) + $route + @(
    'expectview View76', 'expectowner realneck frank', 'expectowner rubaiyat frank',
    'expectglobal mission 2', 'expectglobal phase 0', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-necklace-mission2-native.log"
