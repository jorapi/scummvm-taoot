# SPDX-License-Identifier: GPL-3.0-or-later
# A normal final-dollar loss closes blackjack without offering a new bet.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-cash-hand2-hit1.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-bankruptcy-pending.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal playercash 1','expectglobal playerbet 1','expectglobal playertotal 18',
    'clickon button stay','expectglobal playercash 0','savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-bankruptcy-pending-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-bankruptcy-pending.dfsv" -Checkpoint "$root/build/save-dust-bankruptcy.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'waitglobal playcards 0 60000','expectflat mainpanel','expectglobal playercash 0','expectglobal playerdeath ','savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-bankruptcy-native.log"
