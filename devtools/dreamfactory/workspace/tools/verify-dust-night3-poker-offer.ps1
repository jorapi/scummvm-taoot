# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Mezamee's offer and take the authored decline branch into the bounty plot.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-night3-reconciled.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" NITE B11 west G8 west)
if ($LASTEXITCODE) { throw 'Cannot plan night saloon approach' }
$saloon = @(& node "$PSScriptRoot/dust-route.mjs" SALLOWER D1 west C2 east)
if ($LASTEXITCODE) { throw 'Cannot plan poker table approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-night3-poker-offer.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'mez.pup:102'
) -Actions (@('expectglobal phase 1','expectactorowner jones friend','expectowner tstone stranger') + $town + @(
    'click 270 100','expectowner door saloon','key uparrow','expectset sallower'
) + $saloon + @('clickon prop gamblers','expectflat mainpanel','expectglobal phase 2',
    'expectglobal mezphase 2','expectowner tbird mez','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night3-poker-offer-native.log"
