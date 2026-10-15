# SPDX-License-Identifier: GPL-3.0-or-later
# Leave the saloon after earning the authored six-dollar story requirement.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-casino-result.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" SALLOWER C4 west D1 east
if ($LASTEXITCODE) { throw 'Cannot plan saloon exit' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-night-street.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal phase 2','expectglobalge playercash 6'
) + @($route) + @(
    'click 270 100','expectowner door salout','key uparrow','expectset town',
    'expectglobal phase 3','expectowner ring stranger','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night-street-native.log"
