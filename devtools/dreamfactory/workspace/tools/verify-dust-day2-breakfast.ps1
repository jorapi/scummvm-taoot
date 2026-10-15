# SPDX-License-Identifier: GPL-3.0-or-later
# Meet Laurel at breakfast and hear her information about the Kid.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day2-wakeup.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$upper = & node "$PSScriptRoot/dust-route.mjs" HOTUPPER C4 north C1 north
if ($LASTEXITCODE) { throw 'Cannot plan upstairs exit' }
$stairs = & node "$PSScriptRoot/dust-route.mjs" HOTUPPER C1 west D1 south
if ($LASTEXITCODE) { throw 'Cannot plan stairs approach' }
$lower = & node "$PSScriptRoot/dust-route.mjs" HOTLOWER D3 south B3 west
if ($LASTEXITCODE) { throw 'Cannot plan breakfast approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day2-breakfast.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('buick.pup:101','buick.pup:103','buick.pup:105','laurel.pup:101','laurel.pup:201','laurel.pup:101','laurel.pup:102','laurel.pup:301','laurel.pup:101') -Actions (@(
    'expectglobal day 2','expectglobal clock 1','expectglobal phase 1'
) + @($upper) + @('key leftarrow','expectview west','expectglobal buickphase 1') + @($stairs) + @('key uparrow','expectset hotlower','expectscene Scene D3','expectview south') + @($lower) + @(
    'click 260 230','expectglobal laurelphase 1','expectglobal oonakidstory 1',
    'expectglobal playerdeath ','click 260 230','expectglobal phase 2',
    'expectflat flat 1','savecheckpoint','targets'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-day2-breakfast-native.log"
