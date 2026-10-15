# SPDX-License-Identifier: GPL-3.0-or-later
# Show the prescription to Watson and report his departure to Doc Rodham.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-prescription.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$watson = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G5 east G8 east)
if ($LASTEXITCODE) { throw 'Cannot plan Watson approach' }
$doctor = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G8 east G5 west)
if ($LASTEXITCODE) { throw 'Cannot plan doctor return' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-prescription-watson.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'doc.pup:101'
) -Actions (@('expectglobal docphase 2','expectowner rx stranger','expectglobal handitem rx',
    'key rightarrow','key rightarrow','expectview east','click 250 150','expectowner door doc2',
    'key uparrow','expectset town'
) + $watson + @('dragto prop rx actor watson','expectglobal docphase 3') + $doctor + @(
    'click 250 150','expectowner door doctor','key uparrow','expectset doctor1','clickon actor doc',
    'expectglobal docphase 4','expectowner rx stranger','expectpropdeg rx 0','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-prescription-watson-native.log"
