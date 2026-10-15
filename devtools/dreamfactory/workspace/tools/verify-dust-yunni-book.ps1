# SPDX-License-Identifier: GPL-3.0-or-later
# Receive Oona's Yunni book in the saloon on the third afternoon.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day3-afternoon.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G9 north G8 west)
if ($LASTEXITCODE) { throw 'Cannot plan saloon approach' }
$saloon = @(& node "$PSScriptRoot/dust-route.mjs" SALLOWER D1 west D3 south)
if ($LASTEXITCODE) { throw 'Cannot plan Oona approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-yunni-book.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'oona.pup:101','oona.pup:103'
) -Actions (@('expectglobal day 3','expectglobal clock 2','expectglobal phase 0') + $town + @(
    'click 270 100','expectowner door saloon','key uparrow','expectset sallower',
    'expectscene Scene D1','expectview west'
) + $saloon + @('clickon actor oona','expectowner yunnibook stranger','expectglobal oonaphase 1',
    'expectowner pages stranger','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-yunni-book-native.log"
