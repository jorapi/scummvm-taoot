# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Laurel's second-night conversation and preserve the friendly response.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day2-night.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = & node "$PSScriptRoot/dust-route.mjs" NITE G12 east G5 east
if ($LASTEXITCODE) { throw 'Cannot plan night hotel approach' }
$lobby = & node "$PSScriptRoot/dust-route.mjs" HOTLOWER A1 east B2 south
if ($LASTEXITCODE) { throw 'Cannot plan Laurel approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-night2-laurel.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('laurel.pup:102','laurel.pup:101','laurel.pup:103','laurel.pup:104') -Actions (@(
    'expectglobal day 2','expectglobal clock 3','expectglobal phase 0',
    'key rightarrow','key rightarrow','expectview west','click 250 150',
    'expectowner door lock','key uparrow','expectset town','expectscene Scene G12','expectview east'
) + @($town) + @(
    'click 250 150','expectowner door hotel','key uparrow','expectset hotlower'
) + @($lobby) + @(
    'clickon actor laurel','expectglobal phase 1','expectglobal laurelgood 1',
    'expectowner badge stranger','expectglobal playerdeath ','savecheckpoint','targets'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night2-laurel-native.log"
