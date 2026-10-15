# SPDX-License-Identifier: GPL-3.0-or-later
# Begin Georgia's necklace quest through normal boat-deck navigation/dialogue.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-mission2.dfsv", [int]$TimeoutSeconds = 600, [switch]$Probe, [ValidateRange(0,3)][int]$MissionPhase = 0)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$name = if ($Probe) { "georgia-sprite-probe" } else { "georgia-necklace" }
function Get-NecklaceRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-$name.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices $(if ($Probe) { @() } else { @('ga1.pup:101','ga1.pup:104','ga1.pup:104','ga1.pup:105') }) -Actions (@(
    'expectglobal mission 2', "expectglobal phase $MissionPhase", 'expectglobal neckphase 0',
    'clickon prop map', 'key t', 'expectflat Map 1', 'clickon button Button27',
    'expectset gstair1', 'expectview View47'
) + @(Get-NecklaceRoute GSTAIR1 View47 View43) + @(
    'clickon painting door', 'key uparrow', 'expectset veststbd', 'expectview View18',
    'clickon painting door', 'key uparrow', 'expectset deckbd', 'expectview View110'
) + @(Get-NecklaceRoute DECKBD View110 View255) + @($(if ($Probe) { 'savecheckpoint' } else { @(
    'clickon actor ga', 'expectglobal neckphase 1', 'expectowner fakeneck frank',
    'expectglobal mission 2', "expectglobal phase $MissionPhase", 'savecheckpoint'
) })))
Copy-Item "$root/build/boot-full.log" "$root/build/full-$name-native.log"
