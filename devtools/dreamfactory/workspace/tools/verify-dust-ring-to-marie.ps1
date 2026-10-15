# SPDX-License-Identifier: GPL-3.0-or-later
# Ask Fearwitt for the pledged ring after hearing Jones's request.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-jones-ring-story.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G15 north G5 east)
if ($LASTEXITCODE) { throw 'Cannot plan hotel return' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-ring-marie.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'fear.pup:202','fear.pup:103'
) -Actions (@('expectglobal jonesringstory 1','expectowner ring fear') + $town + @(
    'click 250 150','expectowner door hotel','key uparrow','expectset hotlower',
    'expectscene Scene A1','expectview east','clickon actor fear','expectowner ring marie',
    'expectowner pages stranger','expectglobal day 3','expectglobal clock 1',
    'expectglobal phase 2','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-ring-marie-native.log"
