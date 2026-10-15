# SPDX-License-Identifier: GPL-3.0-or-later
# Mix the authored prescription using the pharmacy's ingredient buttons/movies.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-prescription-watson.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G5 east G9 east)
if ($LASTEXITCODE) { throw 'Cannot plan pharmacy approach' }
$counter = @(& node "$PSScriptRoot/dust-route.mjs" APOTH C2 west A2 north)
if ($LASTEXITCODE) { throw 'Cannot plan pharmacy counter approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-medicine.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectglobal docphase 4','expectowner rx stranger','key rightarrow','key rightarrow','expectview east',
    'click 250 150','expectowner door doc2','key uparrow','expectset town'
) + $town + @('click 250 150','expectowner door apoth','key uparrow','expectset drugs') + $counter + @(
    'clickon prop bottles','expectflat Flat 0','expectglobal bottles 0,0,0,0,0,0,0,0,',
    'clickon button cactus','expectflat Flat 1','expectglobal bottles 1,0,0,0,0,0,0,0,',
    'clickon button rose','expectglobal bottles 1,1,0,0,0,0,0,0,',
    'clickon button shroom','expectglobal bottles 1,1,0,0,1,0,0,0,',
    'clickon button armwort','expectglobal bottles 1,1,0,0,1,0,1,0,',
    'clickon button pinto','expectglobal bottles 1,1,0,0,1,0,1,1,',
    'clickon button accept','expectflat mainpanel','expectglobal docphase 6',
    'expectowner rx stranger','expectpropdeg rx 1','expectowner yunnibook stranger',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-medicine-native.log"
