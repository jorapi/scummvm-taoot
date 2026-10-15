# SPDX-License-Identifier: GPL-3.0-or-later
# Pick up the day-two pie from its authored town placement.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-well-gun.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" TOWN E12 south H7 east
if ($LASTEXITCODE) { throw 'Cannot plan pie approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-pie.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal day 2','expectglobal clock 1','expectowner gun stranger'
) + @($route) + @(
    'clickon prop pie','expectowner pie stranger','expectglobal handitem pie',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-pie-native.log"
