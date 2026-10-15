# SPDX-License-Identifier: GPL-3.0-or-later
# Accept Bloodstone's cigar and follow his authored invitation into the hotel.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-night-street.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" NITE G8 east I7 east
if ($LASTEXITCODE) { throw 'Cannot plan Bloodstone street approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-blood-hotel.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('blood.pup:102','blood.pup:103') -Actions (@(
    'expectglobal day 1','expectglobal clock 3','expectglobal phase 3'
) + @($route) + @(
    'idle 700','dumpactor blood','clickon actor blood','expectglobal phase 4',
    'expectset hotlower','expectowner cigar stranger','expectowner ring stranger',
    'expectglobal playerdeath ','savecheckpoint','targets'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-blood-hotel-native.log"
