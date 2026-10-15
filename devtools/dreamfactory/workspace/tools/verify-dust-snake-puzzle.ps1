# SPDX-License-Identifier: GPL-3.0-or-later
# Solve the actual randomized rotor arrangement through physical pointer drags.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-snake-puzzle-entry.dfsv", [string]$EntryLog = "$PSScriptRoot/../build/dust-snake-puzzle-entry-native.log", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$a = @(& node "$PSScriptRoot/dust-snake-route.mjs" $EntryLog)
if ($LASTEXITCODE) { throw 'Cannot plan the native snake rotor arrangement' }
$a += @('clickon button quit','expectglobal snakepuzzle done','expectglobal flutepuzzle done',
    'expectset hub','expectflat mainpanel','expectglobal day 4','expectglobal clock 3',
    'expectglobal phase 0','expectowner blade snake','expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-snake-puzzle-solved.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-snake-puzzle-solved-native.log"
