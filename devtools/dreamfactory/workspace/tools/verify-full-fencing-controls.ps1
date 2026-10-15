# SPDX-License-Identifier: GPL-3.0-or-later
# Check attacks, pointer defense and an active-bout checkpoint. No state injection.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-fencing-ready.dfsv", [int]$TimeoutSeconds = 60)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$checkpoint = "$root/build/save-full-fencing-controls.dfsv"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint $checkpoint -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectflat fence 8', 'click 256 359', 'expectglobal fighting 1',
    'click 400 100', 'expectglobal attacktot 1', 'expectflat fence 9',
    'click 400 100', 'expectglobal attacktot 2', 'expectflat fence 10',
    'move 100 100', 'waitglobal playerblock left 10000', 'expectpropview player defend',
    'expectglobal fighting 1', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-fencing-controls-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $checkpoint -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectset squash', 'expectflat fence 10', 'expectglobal fighting 1',
    'expectglobal attacktot 2', 'expectpropview player defend',
    'move 100 100', 'waitglobal playerblock left 10000', 'expectglobal fighting 1'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-fencing-controls-restore-native.log"
