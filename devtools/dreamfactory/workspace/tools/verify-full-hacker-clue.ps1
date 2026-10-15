# SPDX-License-Identifier: GPL-3.0-or-later
# Follow the bath clue to the third-class rope and Hacker's authored encounter.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-rubiclue.dfsv", [int]$TimeoutSeconds = 360)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-HackerRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-hacker-clue.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 3', 'expectglobal phase 0', 'expectowner rubiclue frank',
    'clickon prop map', 'key e', 'expectflat Map 6', 'clickon button Button43',
    'expectset turkstrs', 'expectview View23', 'clickon painting door',
    'key uparrow', 'expectset scot1', 'expectview View45'
) + @(Get-HackerRoute SCOT1 View45 View59) + @(
    'clickon painting door', 'key uparrow', 'expectset scot2', 'expectview View15'
) + @(Get-HackerRoute SCOT2 View15 View29) + @(
    'clickon painting door', 'key uparrow', 'expectset scot3', 'expectview View15'
) + @(Get-HackerRoute SCOT3 View15 View25) + @(
    'clickon painting hitrope', 'expectglobal hackphase 1',
    'expectowner rubiclue xxxfrank', 'expectowner hackclue frank',
    'expectglobal mission 3', 'expectglobal phase 0',
    'savecheckpoint', 'waitactorwalk hack 0 60000', 'savecheckpoint'
)) -Choices @('hack1.pup:101', 'hack1.pup:101', 'hack1.pup:103', 'hack1.pup:104') -ClickMovies @('scotrope.mov')
Copy-Item "$root/build/boot-full.log" "$root/build/full-hacker-clue-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-hacker-clue.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectset scot3', 'expectview View22', 'expectglobal mission 3', 'expectglobal phase 0',
    'expectglobal hackphase 1', 'expectowner rubiclue xxxfrank', 'expectowner hackclue frank',
    'expectpropview bag lightclosed', 'clickon prop bag', 'idle 20',
    'expectflat inven 1', 'clickon prop hackclue',
    'expectglobal handitem hackclue', 'clickon button examine', 'expectflat inven 1',
    'clickon button ok', 'expectflat main 1', 'expectowner hackclue frank'
) -ClickMovies @('hackclue.mov')
Copy-Item "$root/build/boot-full.log" "$root/build/full-hacker-clue-restore-native.log"
