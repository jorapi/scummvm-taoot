# SPDX-License-Identifier: GPL-3.0-or-later
# Start a normal $2 blackjack hand from Trotter's departure checkpoint.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-trotter-departure.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-blackjack-active.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('jan.pup:101','jan.pup:101','jan.pup:101','jan.pup:111') -Actions @(
    'expectscene Scene D3','expectview west','key uparrow','expectscene Scene C3',
    'key leftarrow','expectview south','key uparrow','expectscene Scene C4',
    'key rightarrow','expectview west','clickon prop blackjack','expectflat Flat 2',
    'expectglobal playerbet 2','savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-blackjack-active-native.log"
