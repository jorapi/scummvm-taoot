# SPDX-License-Identifier: GPL-3.0-or-later
# Follow Max's lead to the authored engine-room ambush and fight stage.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-turbine2.dfsv", [int]$TimeoutSeconds = 360)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-FightRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-vlad-fight-ready.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 3', 'expectglobal phase 1', 'expectactorowner csea thanks2',
    'expectset control', 'expectview View24'
) + @(Get-FightRoute CONTROL View24 View32) + @(
    'clickon painting door', 'key uparrow', 'expectset engine', 'expectview View66'
) + @(Get-FightRoute ENGINE View66 View119) + @(
    'waitglobal phase 2 60000', 'waitglobal lockevents 0 30000',
    'expectglobal mission 3', 'expectglobal phase 2', 'expectflat flat 0',
    'expectglobal fightover 0', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-vlad-fight-ready-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-vlad-fight-ready.dfsv" -Checkpoint "$root/build/save-full-vlad-fight-controls.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectflat flat 0', 'expectglobal phase 2', 'expectglobal vladpower 512',
    'click 200 100', 'expectglobal firstpunch jab', 'expectglobal vladpower 511',
    'click 300 100', 'expectglobal vladpower 505',
    'click 100 100', 'expectglobal firstpunch cross', 'expectglobal vladpower 465', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-vlad-fight-controls-native.log"
$actions = @('expectflat flat 0', 'expectglobal mission 3', 'expectglobal phase 2', 'expectglobal vladpower 465')
for ($round = 0; $round -lt 11; ++$round) {
    $actions += @('click 200 100', 'click 300 100', 'click 100 100')
}
$actions += @(
    'waitglobal vladpower 0 15000', 'expectflat main 1', 'expectactorowner vlad lostfight',
    'expectglobal mission 3', 'expectglobal phase 2', 'savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-vlad-fight-controls.dfsv" -Checkpoint "$root/build/save-full-vlad-defeated.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions $actions
Copy-Item "$root/build/boot-full.log" "$root/build/full-vlad-fight-win-native.log"
