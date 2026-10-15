# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Buick's breakfast conversation, receive the hairpin and finish the meal.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day3-wakeup.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$upper = & node "$PSScriptRoot/dust-route.mjs" HOTUPPER C4 north D1 south
if ($LASTEXITCODE) { throw 'Cannot plan upstairs exit' }
$lower = & node "$PSScriptRoot/dust-route.mjs" HOTLOWER D3 south B3 west
if ($LASTEXITCODE) { throw 'Cannot plan breakfast approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day3-breakfast.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('buick.pup:201') -Actions (@(
    'expectglobal day 3','expectglobal clock 1','expectglobal phase 1'
) + @($upper) + @('key uparrow','expectset hotlower') + @($lower) + @(
    'click 260 230','expectowner hairpin stranger','expectglobal buickphase 1',
    'click 260 230','expectglobal phase 2','expectflat flat 1',
    'clickon button s','clickon button b','expectflat flat 4','click 500 300',
    'expectflat mainpanel','expectowner biscuits stranger','expectowner sugarcubes stranger',
    'expectglobal playerdeath ','savecheckpoint','targets'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-day3-breakfast-native.log"
