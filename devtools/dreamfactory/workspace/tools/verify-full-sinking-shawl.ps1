# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Penny's sinking briefing, then walk to Claris and obtain her shawl.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-mission4.dfsv", [int]$TimeoutSeconds = 480)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-ShawlRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To 1
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-sinking-brief.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 4', 'expectglobal phase 0', 'expectset c73', 'expectview View63'
) + @(Get-ShawlRoute C73 View63 View55) + @(
    'clickon painting door', 'expectglobal pennyphase 1', 'expectowner notebook vlad',
    'expectowner painting frank', 'expectowner rubaiyat frank', 'savecheckpoint'
)) -ClickMovies @('lenin.mov')
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-brief-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-sinking-brief.dfsv" -Checkpoint "$root/build/save-full-sinking-shawl.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'clickon painting door', 'key uparrow', 'expectset hallc', 'expectview View35'
) + @(Get-ShawlRoute HALLC View35 View22) + @(
    'clickon painting door', 'key uparrow', 'expectset stair1c2', 'expectview View138'
) + @(Get-ShawlRoute STAIR1C2 View138 View19) + @(
    'expectset stair1c1', 'expectview View107'
) + @(Get-ShawlRoute STAIR1C1 View107 View61) + @(
    'clickon painting door', 'key uparrow', 'expectset lnghall', 'expectview View16'
) + @(Get-ShawlRoute LNGHALL View16 View12) + @(
    'clickon painting door', 'key uparrow', 'expectowner shawl frank',
    'expectglobal clarisphase 1', 'expectglobal mission 4', 'savecheckpoint'
)) -Choices @('claris2.pup:101', 'claris2.pup:102')
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-shawl-native.log"
