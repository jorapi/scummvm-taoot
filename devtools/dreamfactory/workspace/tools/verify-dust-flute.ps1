# SPDX-License-Identifier: GPL-3.0-or-later
# Deliver the mixed medicine and receive Trotter's flute through normal dialogue.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-medicine.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exit = @(& node "$PSScriptRoot/dust-route.mjs" APOTH A2 north C2 east)
if ($LASTEXITCODE) { throw 'Cannot plan pharmacy exit' }
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G9 west G5 west)
if ($LASTEXITCODE) { throw 'Cannot plan medicine delivery' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-medicine-delivered.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'doc.pup:101'
) -Actions (@('expectglobal docphase 6','expectowner rx stranger','expectpropdeg rx 1') + $exit + @(
    'click 250 150','expectowner door pharm','key uparrow','expectset town'
) + $town + @('click 250 150','expectowner door doctor','key uparrow','expectset doctor1',
    'clickon actor doc','expectglobal docphase 7','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-medicine-delivered-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-medicine-delivered.dfsv" -Checkpoint "$root/build/save-dust-flute.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'trotter.pup:101'
) -Actions @(
    'expectglobal docphase 7','key rightarrow','key rightarrow','expectview east',
    'click 250 150','expectowner door doc2','key uparrow','expectset town',
    'key rightarrow','key rightarrow','expectview west','click 250 150','expectowner door doctor',
    'key uparrow','expectset doctor1','clickon actor doc','expectglobal docphase 8',
    'click 250 150','expectowner door doc1','key uparrow','expectset doctor2','expectview south',
    'clickon actor trotter','expectowner flute stranger','expectglobal docphase 9','expectglobal trotterphase 1',
    'expectowner yunnibook stranger','expectglobal day 3','expectglobal clock 2',
    'expectglobal playerdeath ','savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-flute-native.log"
