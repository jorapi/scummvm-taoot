# SPDX-License-Identifier: GPL-3.0-or-later
# Reach Leroy and enter the range through his authored offer.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day3-breakfast.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$lobby = @(& node "$PSScriptRoot/dust-route.mjs" HOTLOWER B3 west A1 west)
if ($LASTEXITCODE) { throw 'Cannot plan hotel exit' }
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G5 west K11 south)
if ($LASTEXITCODE) { throw 'Cannot plan range approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day3-range-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @('leroy.pup:101') -Actions (@(
    'expectglobal day 3','expectglobal clock 1','expectglobal phase 2'
) + $lobby + @('click 250 150','expectowner door hotout','key uparrow','expectset town') + $town + @(
    'clickon actor leroy','expectset target','expectglobal leroyphase 3',
    'expectglobal bulletcount 6','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-day3-range-entry-native.log"
