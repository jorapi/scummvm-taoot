# SPDX-License-Identifier: GPL-3.0-or-later
# Enter poker from the saloon and preserve the first normally dealt hand.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-trotter-departure.dfsv", [int]$TimeoutSeconds = 420, [string]$StartCell = "D3", [ValidateSet("north","south","east","west")][string]$StartView = "west")
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" SALLOWER $StartCell $StartView C2 east
if ($LASTEXITCODE) { throw 'Cannot plan poker table route' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-poker-ready.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('mez.pup:101','mez.pup:101') -Actions (@(
    'expectglobal day 1','expectglobal phase 2','expectowner cards stranger'
) + @($route) + @(
    'clickon prop gamblers','expectflat Flat 0','expectglobal playerphase 1',
    'expectglobal playerbet 1','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-poker-ready-native.log"
