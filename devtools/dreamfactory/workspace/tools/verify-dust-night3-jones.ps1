# SPDX-License-Identifier: GPL-3.0-or-later
# Find Jones at the authored "chicken" scene and hear about his quarrel.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-tstone.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$bank = @(& node "$PSScriptRoot/dust-route.mjs" BANK D1 west D2 east)
if ($LASTEXITCODE) { throw 'Cannot plan bank exit' }
$town = @(& node "$PSScriptRoot/dust-route.mjs" NITE G6 east chicken west)
if ($LASTEXITCODE) { throw 'Cannot plan Jones approach' }
$meeting = @(& node "$PSScriptRoot/dust-route.mjs" NITE chicken west B11 west)
if ($LASTEXITCODE) { throw 'Cannot plan Jones meeting point' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-night3-jones.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'jones.pup:101','jones.pup:102','jones.pup:201'
) -Actions (@('expectglobal day 3','expectglobal clock 3','expectglobal phase 0','expectowner tstone stranger') + $bank + @(
    'click 250 150','expectowner door dollar','key uparrow','expectset town'
) + $town + @('waitactorstar jones town.seek2 60000') + $meeting + @('idle 40','expectglobal jonesphase 1',
    'expectglobal phase 0','expectowner tstone stranger','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night3-jones-native.log"
