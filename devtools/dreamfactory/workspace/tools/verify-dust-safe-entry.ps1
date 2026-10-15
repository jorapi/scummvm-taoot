# SPDX-License-Identifier: GPL-3.0-or-later
# Use Buick's earned hairpin to enter the bank and approach its safe at night.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day3-night.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" NITE G7 west G6 west)
if ($LASTEXITCODE) { throw 'Cannot plan night bank approach' }
$bank = @(& node "$PSScriptRoot/dust-route.mjs" BANK D2 west D1 west)
if ($LASTEXITCODE) { throw 'Cannot plan safe approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-safe-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectglobal day 3','expectglobal clock 3','expectglobal phase 0','expectowner hairpin stranger'
) + $town + @('click 250 150','expectset bank','expectscene Scene D2','expectview west') + $bank + @(
    'click 250 150','expectglobal combo -1,-1,-1,','expectglobal turnright 1','expectglobal curtwist 1',
    'dumpprop spin','targets','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-safe-entry-native.log"
