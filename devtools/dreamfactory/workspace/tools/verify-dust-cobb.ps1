# SPDX-License-Identifier: GPL-3.0-or-later
# Approach the Chinese store, answer Cobb's threat, draw and fire normally.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-flippo-alarm.dfsv", [int]$TimeoutSeconds = 300, [switch]$ApproachOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" TOWN D8 east G13 north
if ($LASTEXITCODE) { throw 'Cannot plan Chinese store approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-cobb-approach.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal day 2','expectglobal clock 2','expectglobal phase 1',
    'clickon prop avatar','expectflat avatar','clickon prop gun','clickon button ok',
    'expectglobal handitem gun','key leftarrow','expectview east','click 250 150',
    'expectowner door flipout','key uparrow','expectset town'
) + @($route) + @('expectglobal phase 1','expectglobal playerdeath ','savecheckpoint','targets'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-cobb-approach-native.log"
if ($ApproachOnly) { return }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-cobb-approach.dfsv" -Checkpoint "$root/build/save-dust-cobb-defeated.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('cobb.pup:101') -Actions @(
    'expectglobal phase 1','expectglobal handitem gun','clickon actor cobb',
    'expectglobal phase 2','clickon prop gun','waitpropview gunhand idle 10000',
    'clickon actor cobb','expectglobal phase 3','expectowner bullets stranger',
    'expectglobal bulletcount 5','expectglobal playerdeath ','savecheckpoint','targets'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-cobb-defeated-native.log"
