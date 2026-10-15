# SPDX-License-Identifier: GPL-3.0-or-later
# Meet Ned in the livery and let his authored departure enable the well quest.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day2-after-breakfast.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$lobby = & node "$PSScriptRoot/dust-route.mjs" HOTLOWER B3 west A1 west
if ($LASTEXITCODE) { throw 'Cannot plan hotel exit' }
$town = & node "$PSScriptRoot/dust-route.mjs" TOWN G5 west J6 east
if ($LASTEXITCODE) { throw 'Cannot plan livery approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day2-livery.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('ned.pup:104') -Actions (@(
    'expectglobal day 2','expectglobal clock 1','expectglobal phase 2'
) + @($lobby) + @(
    'click 260 160','expectowner door hotout','key uparrow','expectset town',
    'expectscene Scene G5','expectview west'
) + @($town) + @(
    'click 250 150','expectowner door livery','key uparrow','expectset livery',
    'expectscene Scene D2','expectview west','key rightarrow','expectview north',
    'waitglobal nedphase 2 60000','expectglobal playerdeath ','savecheckpoint','targets'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-day2-livery-native.log"
