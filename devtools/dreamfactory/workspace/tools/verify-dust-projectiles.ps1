# SPDX-License-Identifier: GPL-3.0-or-later
# Save a naturally launched shot in flight, let it expire, restore and resume.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day3-range-entry.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-range-active-ball.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions @(
    'expectset target','expectballs 0','click 256 50','expectglobal targetshotcount 1',
    'expectballs 1','dumpballs','savecheckpoint','idle 90','expectballs 0',
    'loadcheckpoint','expectballs 1','dumpballs','idle 90','expectballs 0','expectglobal playerdeath '
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-range-ball-save-native.log"
