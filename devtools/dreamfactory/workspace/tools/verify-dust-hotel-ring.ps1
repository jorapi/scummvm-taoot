# SPDX-License-Identifier: GPL-3.0-or-later
# Trade the ring for a hotel room through the normal inventory drag interface.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-blood-hotel.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-hotel-ring.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('fear.pup:101') -Actions @(
    'expectset hotlower','expectglobal day 1','expectglobal phase 4','expectowner ring stranger',
    'clickon prop avatar','expectflat avatar','clickon prop ring','expectglobal handitem ring',
    'clickon button ok','expectflat mainpanel','dragto prop ring actor fear',
    'expectowner ring fear','expectglobal phase 5','expectglobal playercash 6',
    'expectglobal playerdeath ','savecheckpoint','targets'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-hotel-ring-native.log"
