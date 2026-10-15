# SPDX-License-Identifier: GPL-3.0-or-later
# Trade the pie to Sidewinder through a normal inventory drag.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-pie.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" TOWN H7 east A7 south
if ($LASTEXITCODE) { throw 'Cannot plan undertaker approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-boots.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('side.pup:102','side.pup:101','side.pup:101','side.pup:102') -Actions (@(
    'expectglobal day 2','expectglobal clock 1','expectowner pie stranger'
) + @($route) + @(
    'click 250 150','expectowner door undertak','key uparrow','expectset under',
    'expectscene Scene A2','expectview east','key leftarrow','expectview north',
    'expectglobal sidephase 1','expectglobal handitem pie','dragto prop pie actor side',
    'expectowner pie side','expectowner boots stranger','expectpropdeg boots 1',
    'expectowner gun stranger','expectglobal playerdeath ','savecheckpoint','targets'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-boots-native.log"
