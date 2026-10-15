# SPDX-License-Identifier: GPL-3.0-or-later
# Collect Mezamee's authored post-bounty reward without gambling.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-bounty-trotter.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" NITE G9 north G8 west)
if ($LASTEXITCODE) { throw 'Cannot plan saloon approach' }
$saloon = @(& node "$PSScriptRoot/dust-route.mjs" SALLOWER D1 west C2 east)
if ($LASTEXITCODE) { throw 'Cannot plan poker table approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-tbird.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'mez.pup:101','mez.pup:101','mez.pup:102'
) -Actions (@('expectglobal fighton 0','expectglobal phase 3') + $town + @(
    'click 270 100','expectowner door saloon','key uparrow','expectset sallower','expectglobal phase 4'
) + $saloon + @('clickon prop gamblers','expectflat mainpanel','expectowner tbird stranger',
    'expectowner tstone stranger','expectglobal playerdeath ','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-tbird-native.log"
