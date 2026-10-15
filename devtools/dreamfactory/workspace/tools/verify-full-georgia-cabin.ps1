# SPDX-License-Identifier: GPL-3.0-or-later
# Follow Georgia from the boat deck to A deck, then visit cabin B70.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-georgia-necklace.dfsv", [int]$TimeoutSeconds = 600, [switch]$CabinOnly, [switch]$BoatDeckJones)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-GeorgiaRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
if (!$CabinOnly) {
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-georgia-adeck.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices $(@(if ($BoatDeckJones) { 'jones1.pup:101' }) + @('ga1.pup:101')) -Actions (@(
    'expectglobal neckphase 1', 'expectowner fakeneck frank', 'clickon prop map',
    'key a', 'expectflat Map 2', 'clickon button Button34', 'expectset gstair2', 'expectview View37'
) + @(Get-GeorgiaRoute GSTAIR2 View37 View36) + @(
    'clickon painting door', 'key uparrow', 'expectset decka', 'expectview View424'
) + @(Get-GeorgiaRoute DECKA View424 View373) + @(
    'clickon actor ga', 'waitglobal neckphase 2', 'expectowner fakeneck frank', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-georgia-adeck-native.log"
}
$cabinRestore = if ($CabinOnly) { $RestoreCheckpoint } else { "$root/build/save-full-georgia-adeck.dfsv" }
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $cabinRestore -Checkpoint "$root/build/save-full-georgia-b70.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('ga1.pup:102','ga1.pup:101','ga1.pup:101','ga1.pup:101') -Actions (@(
    'clickon prop map', 'key b', 'expectflat Map 3', 'clickon button Button38',
    'expectset gstair3', 'expectview View44', 'expectglobal savedeck b'
) + @(Get-GeorgiaRoute GSTAIR3 View44 View43) + @(
    'clickon painting door', 'key uparrow', 'expectset hallb', 'expectview View21', 'expectglobal hallside port'
) + @(Get-GeorgiaRoute HALLB View21 View48) + @(
    'clickon painting door', 'key uparrow', 'expectset b70'
) + @(Get-GeorgiaRoute B70 View19 View24) + @(
    'waitglobal neckphase 5 60000',
    'waitset hallb 60000', 'expectview View49', 'expectowner fakeneck frank', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-georgia-b70-native.log"
