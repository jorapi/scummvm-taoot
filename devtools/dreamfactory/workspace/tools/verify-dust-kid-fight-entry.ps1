# SPDX-License-Identifier: GPL-3.0-or-later
# Return outside after Jones's warning to begin the Kid's authored attack.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day4-saloon.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$r = @(& node "$PSScriptRoot/dust-route.mjs" SALLOWER C3 west D1 east)
if ($LASTEXITCODE) { throw 'Cannot plan saloon exit' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-kid-fight-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectglobal day 4','expectglobal clock 1','expectglobal phase 1'
) + $r + @('click 250 150','expectowner door salout','key uparrow',
    'expectset town','expectglobal day 4','expectglobal clock 2','expectglobal phase 2',
    'expectglobal fightphase 1','expectglobal fighton 1','expectglobal playerdeath ','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-kid-fight-entry-native.log"
