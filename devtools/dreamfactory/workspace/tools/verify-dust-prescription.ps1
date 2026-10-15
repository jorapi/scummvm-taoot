# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Doc Rodham's request and collect the prescription on the third afternoon.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-yunni-book.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$saloon = @(& node "$PSScriptRoot/dust-route.mjs" SALLOWER D3 south D1 east)
if ($LASTEXITCODE) { throw 'Cannot plan saloon exit' }
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G8 east G5 west)
if ($LASTEXITCODE) { throw 'Cannot plan doctor approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-prescription.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'doc.pup:101','doc.pup:102','doc.pup:103'
) -Actions (@('expectglobal day 3','expectglobal clock 2','expectowner yunnibook stranger') + $saloon + @(
    'click 250 150','expectowner door salout','key uparrow','expectset town'
) + $town + @('click 250 150','expectowner door doctor','key uparrow','expectset doctor1',
    'expectscene Scene B1','expectview west','clickon actor doc','expectglobal docphase 2',
    'expectowner rx stranger','expectowner yunnibook stranger','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-prescription-native.log"
