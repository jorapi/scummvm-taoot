# SPDX-License-Identifier: GPL-3.0-or-later
# Jones's first-night warning runs on his ordinary street arrival callback.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-hotel-key.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-jones-warning.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal phase 7','key rightarrow','key rightarrow','expectview west',
    'click 260 160','key uparrow','expectset town','expectscene Scene G5',
    'expectview south','expectglobal phase 8','idle 250',
    'expectglobal lockevents 0','expectglobal playerdeath ','savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-jones-warning-native.log"
