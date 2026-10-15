# SPDX-License-Identifier: GPL-3.0-or-later
# Give the recovered ring to Jones with the ordinary inventory drag gesture.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-ring-returned.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN J9 east G15 north)
if ($LASTEXITCODE) { throw 'Cannot plan Jones return' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-ring-jones.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectowner ring stranger','expectglobal handitem ring','expectowner pages stranger'
) + $town + @('waitactorstar jones town.leroy1 60000','dragto prop ring actor jones',
    'expectowner ring jones','expectowner pages stranger','expectglobal day 3','expectglobal clock 1',
    'expectglobal phase 2','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-ring-jones-native.log"
