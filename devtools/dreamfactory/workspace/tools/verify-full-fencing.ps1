# SPDX-License-Identifier: GPL-3.0-or-later
# Follow the fencing lead, lose a normal bout and obtain the authored fallback ring.
# Requires the checkpoint produced by verify-full-painting.ps1. Winning remains a separate test.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-painting.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-FencingRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
function Invoke-FencingStage([string]$Name, [string]$Restore, [string[]]$Actions, [string[]]$Choices = @()) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $Restore -Checkpoint "$root/build/save-full-$Name.dfsv" -Steps 1 -Actions $Actions -Choices $Choices -TimeoutSeconds $TimeoutSeconds
    Copy-Item "$root/build/boot-full.log" "$root/build/full-$Name-native.log"
}
Invoke-FencingStage 'cargo-exited' $RestoreCheckpoint (@(
    'expectglobal mission 2', 'expectglobal phase 1', 'click 457 351', 'expectflat main 1'
) + @(Get-FencingRoute BINL View17 View14) + @(
    'clickon painting door', 'key uparrow', 'expectset cargo', 'expectview View17'
) + @(Get-FencingRoute CARGO View17 View18) + @(
    'key uparrow', 'expectglobal hallside port'
) + @(Get-FencingRoute CARGO View18 View101) + @(
    'clickon painting door', 'key uparrow', 'expectset carghall', 'expectview View21'
) + @(Get-FencingRoute CARGHALL View21 View13) + @(
    'key uparrow', 'expectset crew', 'expectview View15'
) + @(Get-FencingRoute CREW View15 View17) + @(
    'clickon painting door', 'key uparrow', 'expectset fore', 'expectview View595',
    'clickon prop map', 'key a', 'expectflat Map 2', 'clickon button Button32',
    'expectset stair1c1', 'expectview View74', 'savecheckpoint'
))
Invoke-FencingStage 'fencing-hint' "$root/build/save-full-cargo-exited.dfsv" (@(
    Get-FencingRoute STAIR1C1 View74 View30
) + @('clickon actor smeth', 'expectowner ring willie', 'expectglobal smethphase 1', 'savecheckpoint')) @('smeth1.pup:888')
Invoke-FencingStage 'fencing-ready' "$root/build/save-full-fencing-hint.dfsv" (@(
    'clickon prop map', 'key d', 'expectflat Map 5', 'clickon button Button42',
    'expectset recept1c', 'expectview View27'
) + @(Get-FencingRoute RECEPT1C View27 View91) + @(
    'clickon painting door', 'key uparrow', 'expectset halld', 'expectview View31'
) + @(Get-FencingRoute HALLD View31 View103) + @(
    'clickon painting door', 'key uparrow', 'expectset sqhall', 'expectview View17'
) + @(Get-FencingRoute SQHALL View17 View42) + @(
    'clickon painting door', 'key uparrow', 'expectset squash', 'expectflat fence 8',
    'expectglobal willphase 201', 'savecheckpoint'
)) @('wilfenc1.pup:101', 'wilfenc1.pup:102')
$bout = @('expectflat fence 8')
for ($point = 0; $point -lt 5; ++$point) {
    # Centered foil leaves both sides open; the authored opponent scores.
    $bout += @('click 256 359', 'waitglobal fighting 0 60000')
}
$bout += @('expectglobal fencecount 1', 'expectactorowner willie lost', 'expectset sqhall', 'expectflat main 1', 'savecheckpoint')
Invoke-FencingStage 'fencing-lost' "$root/build/save-full-fencing-ready.dfsv" $bout @('wilfenc1.pup:102')
Invoke-FencingStage 'ring' "$root/build/save-full-fencing-lost.dfsv" (@(
    'clickon prop map', 'key d', 'expectflat Map 5', 'clickon button Button42',
    'expectset recept1c', 'expectview View27'
) + @(Get-FencingRoute RECEPT1C View27 View25) + @(
    'clickon actor smeth', 'expectowner ring frank', 'expectglobal mission 2', 'expectglobal phase 2', 'savecheckpoint'
))
