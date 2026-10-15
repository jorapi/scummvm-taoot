# SPDX-License-Identifier: GPL-3.0-or-later
# Take the authored breakfast items and close the meal interface normally.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day2-breakfast.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day2-after-breakfast.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal day 2','expectglobal clock 1','expectglobal phase 2','expectflat flat 1',
    'clickon button s','expectflat flat 2','expectowner sugarcubes stranger',
    'clickon button b','expectflat flat 4','expectowner biscuits stranger',
    'click 500 300','expectflat mainpanel','expectset hotlower',
    'expectglobal playerdeath ','savecheckpoint','targets'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-breakfast-items-native.log"
