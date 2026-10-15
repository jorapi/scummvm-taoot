# SPDX-License-Identifier: GPL-3.0-or-later
# Accept the authored sheriff appointment and transition into day-two night.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-cobb-defeated.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day2-night.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('mayor.pup:102','mayor.pup:103') -Actions @(
    'expectglobal day 2','expectglobal clock 2','expectglobal phase 3',
    'expectset town','expectscene Scene G13','expectview north',
    'key uparrow','expectscene Scene G12','key uparrow',
    'expectowner badge stranger','expectactorowner dell jail','expectglobal day 2',
    'expectglobal clock 3','expectglobal phase 0','expectset jail',
    'expectglobal playeraccount 1','expectglobal playerdeath ','savecheckpoint','targets'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-sheriff-native.log"
