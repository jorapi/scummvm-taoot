# SPDX-License-Identifier: GPL-3.0-or-later
# Trotter's day-one conversation and peaceful departure from the saloon.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-trotter-intro.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-trotter-warning.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectset sallower', 'expectglobal trotterphase 1', 'clickon actor trotter',
    'expectglobal trotterphase 2', 'savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-trotter-warning-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-trotter-warning.dfsv" -Checkpoint "$root/build/save-dust-trotter-drink.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('trotter.pup:101','trotter.pup:101','trotter.pup:102') -Actions @(
    'clickon actor trotter', 'expectglobal trotterphase 4', 'expectglobal playercash 4', 'expectglobal playerdeath ', 'savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-trotter-drink-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-trotter-drink.dfsv" -Checkpoint "$root/build/save-dust-trotter-departure.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'clickon actor trotter', 'expectglobal trotterphase 5',
    'clickon actor trotter', 'expectglobal trotterphase 6',
    'clickon actor trotter', 'expectglobal trotterphase 7',
    'expectscene Scene D3', 'expectview west', 'expectglobal playerdeath ', 'savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-trotter-departure-native.log"
