# SPDX-License-Identifier: GPL-3.0-or-later
# Finish the waves, answer the Kid's taunts, then hit the authored movie targets.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-kid-waves-defeated.dfsv", [int]$TimeoutSeconds = 360)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-kid-waves-complete.dfsv" -TimeoutSeconds 90 -Steps 1 -Actions @(
    'key rightarrow','expectview east','waitglobal phase 3 10000',
    'expectglobal fightphase 4','expectglobal playerdeath ','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-kid-waves-complete-native.log"
$r = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G6 east G5 south)
if ($LASTEXITCODE) { throw 'Cannot plan Kid confrontation' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-kid-waves-complete.dfsv" -Checkpoint "$root/build/save-dust-day4-night.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'kid.pup:101','kid.pup:101','kid.pup:101','kid.pup:101','kid.pup:101'
) -MovieClicks @('kiddie.mov:#1','kiddie.mov:#1','kiddie.mov:#1') -Actions ($r + @(
    'expectglobal day 4','expectglobal clock 3','expectglobal phase 0',
    'expectglobal fighton 1','expectowner tbird stranger','expectowner tstone stranger',
    'expectglobal playerdeath ','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-kid-duel-native.log"
