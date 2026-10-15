# SPDX-License-Identifier: GPL-3.0-or-later
# Enter the bank-safe combination with physical pointer arcs and take the stone.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-safe-entry.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dial = @(& node "$PSScriptRoot/dust-safe-route.mjs")
if ($LASTEXITCODE) { throw 'Cannot plan physical dial gestures' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-tstone.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -MovieClicks @(
    'safebox.mov:#1','safebox.mov:#1'
) -Actions ($dial + @('expectglobal curtwist 4','expectglobal turnright 2','clickon button tryopen',
    'expectflat mainpanel','expectset bank','expectowner tstone stranger','expectglobal day 3','expectglobal clock 3',
    'expectowner mask stranger','expectowner yunnibook stranger','expectowner flute stranger',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-tstone-native.log"
