# SPDX-License-Identifier: GPL-3.0-or-later
# Let Ned meet Quist, then retrieve the gun through the well's movie regions.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day2-livery.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" TOWN J6 west E12 south
if ($LASTEXITCODE) { throw 'Cannot plan well approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-well-gun.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -MovieClicks @('wellgun.mov:#1','wellgun.mov:#1') -Actions (@(
    'expectglobal day 2','expectglobal clock 1','expectglobal phase 2','expectglobal nedphase 2',
    'expectset livery','expectscene Scene D2','expectview north',
    'key rightarrow','expectview east','click 250 150','expectowner door horse',
    'key uparrow','expectset town','expectscene Scene J6','expectview west'
) + @($route) + @(
    'waitactorstar ned town.ned2 60000','click 260 220',
    'expectowner gun stranger','expectpropdeg gun 1','expectglobal nedphase 1',
    'expectglobal playerdeath ','savecheckpoint','targets'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-well-gun-native.log"
