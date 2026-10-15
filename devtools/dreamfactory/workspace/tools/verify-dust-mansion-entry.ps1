# SPDX-License-Identifier: GPL-3.0-or-later
# Dismiss Mrs. Macintosh through her authored insult branch and enter the gate.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-flute.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G5 east J9 east)
if ($LASTEXITCODE) { throw 'Cannot plan mansion approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-mansion-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'mwife.pup:201','mwife.pup:201','mwife.pup:201'
) -Actions (@('expectowner flute stranger','expectowner yunnibook stranger','expectglobal docphase 9',
    'key leftarrow','expectview east','click 250 150','expectowner door doc4','key uparrow',
    'expectset doctor1','expectview east','click 250 150','expectowner door doc2','key uparrow','expectset town'
) + $town + @('clickon actor mwife','expectglobal mwifelike -3','click 250 150',
    'expectowner door mayor','key uparrow','expectset mayhall','expectscene Scene C4','expectview north',
    'expectowner flute stranger','expectowner yunnibook stranger','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-mansion-entry-native.log"
