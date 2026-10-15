# SPDX-License-Identifier: GPL-3.0-or-later
# Pick up the afternoon handkerchief from the pharmacy counter.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day2-afternoon.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-pharmacy-entry.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal day 2','expectglobal clock 2','expectglobal phase 0',
    'expectset town','expectscene Scene G9','expectview east',
    'click 250 150','expectowner door apoth','key uparrow','expectset drugs',
    'expectscene Scene C2','expectview west','savecheckpoint','targets'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-pharmacy-entry-native.log"
$route = & node "$PSScriptRoot/dust-route.mjs" APOTH C2 west A2 south
if ($LASTEXITCODE) { throw 'Cannot plan pharmacy counter approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-pharmacy-entry.dfsv" -Checkpoint "$root/build/save-dust-handkerchief.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@($route) + @(
    'clickon prop hankerchief','expectowner hankerchief stranger','expectpropdeg hankerchief 0',
    'expectglobal day 2','expectglobal clock 2','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-handkerchief-native.log"
