# SPDX-License-Identifier: GPL-3.0-or-later
# Carry Jones's apology to Laurel, then return to Jones to reconcile them.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-night3-jones.dfsv", [int]$TimeoutSeconds = 360)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$laurel = @(& node "$PSScriptRoot/dust-route.mjs" NITE B11 west G8 north)
if ($LASTEXITCODE) { throw 'Cannot plan Laurel approach' }
$jones = @(& node "$PSScriptRoot/dust-route.mjs" NITE G8 north B11 west)
if ($LASTEXITCODE) { throw 'Cannot plan Jones return' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-night3-laurel.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'laurel.pup:111','laurel.pup:101','laurel.pup:102','laurel.pup:103','laurel.pup:104'
) -Actions (@('expectglobal jonesphase 1','expectglobal phase 0') + $laurel + @(
    'waitactorstar laurel town.jones1 90000','clickon actor laurel','expectglobal laurelphase 2','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night3-laurel-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-night3-laurel.dfsv" -Checkpoint "$root/build/save-dust-night3-reconciled.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'jones.pup:101'
) -Actions ($jones + @('waitactorstar jones town.seek1 60000','clickon actor jones','waitglobal phase 1 120000',
    'expectglobal phase 1','expectactorowner jones friend','expectowner tstone stranger',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night3-reconciled-native.log"
