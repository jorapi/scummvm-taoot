# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Flippo's warning and Jones's interruption after obtaining equipment.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-bank-deposit.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exit = & node "$PSScriptRoot/dust-route.mjs" BANK D3 west D2 east
if ($LASTEXITCODE) { throw 'Cannot plan bank exit' }
$town = & node "$PSScriptRoot/dust-route.mjs" TOWN G6 east D8 west
if ($LASTEXITCODE) { throw 'Cannot plan newspaper office approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-flippo-alarm.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('flippo.pup:101') -Actions (@(
    'expectglobal day 2','expectglobal clock 2','expectglobal phase 0','expectglobal playeraccount 1'
) + @($exit) + @(
    'click 250 150','expectowner door dollar','key uparrow','expectset town'
) + @($town) + @(
    'click 250 150','expectowner door paper','key uparrow','expectset paper',
    'expectscene Scene B2','expectview west','key leftarrow','expectview south','clickon actor flippo',
    'expectglobal phase 1','expectglobal flippophase 2','expectglobal jonesphase 999',
    'expectowner gun stranger','expectowner boots stranger','expectowner bullets stranger',
    'expectglobal playerdeath ','savecheckpoint','targets'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-flippo-alarm-native.log"
