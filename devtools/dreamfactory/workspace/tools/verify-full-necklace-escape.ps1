# SPDX-License-Identifier: GPL-3.0-or-later
# Leave Sasha's cabin with the real necklace and the replica in the doll.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-necklace-swapped.dfsv", [int]$TimeoutSeconds = 240, [ValidateRange(0,3)][int]$MissionPhase = 0)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/room-route.mjs" full A14 View11 View21
if ($LASTEXITCODE) { throw 'Cannot plan Sasha cabin exit' }
$route = @($route | ForEach-Object { ($_ -split ' # ')[0] })
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-necklace-recovered.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectflat patty 3', 'expectowner realneck frank', 'expectowner fakeneck vlad',
    'clickon button back', 'expectflat patty 1', 'clickon button pattyexit', 'expectflat main 1'
) + $route + @(
    'clickon painting door', 'key uparrow', 'expectset halla', 'expectview View58',
    'expectglobal neckphase 9', 'expectowner realneck frank', 'expectowner fakeneck vlad',
    'expectglobal playerdeath ', 'expectglobal mission 2', "expectglobal phase $MissionPhase", 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-necklace-recovered-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-necklace-recovered.dfsv" -Steps 1 -TimeoutSeconds 60 -Actions @(
    'expectset halla', 'expectview View58', 'expectglobal neckphase 9',
    'expectowner realneck frank', 'expectowner fakeneck vlad', 'key rightarrow', 'expectview View59'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-necklace-restore-native.log"
