# SPDX-License-Identifier: GPL-3.0-or-later
# Enter the store and choose Bolivar's checkers through the authored conversations.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day2-afternoon.dfsv", [ValidateSet('easy','medium','hard')][string]$Difficulty = 'easy', [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$r = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G9 east G10 east)
if ($LASTEXITCODE) { throw 'Cannot plan the store approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-store-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal day 2','expectglobal clock 2'
) + $r + @('click 255 150','expectowner door store','key uparrow','expectset store','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-store-entry-native.log"
$choice = @{easy=103;medium=102;hard=101}[$Difficulty]
$phase = @{easy=9003;medium=9002;hard=9001}[$Difficulty]
$look = @{easy=2;medium=3;hard=4}[$Difficulty]
$ready = "$root/build/save-dust-checkers-$Difficulty-ready.dfsv"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-store-entry.dfsv" -Checkpoint $ready -TimeoutSeconds $TimeoutSeconds -Choices @('bolivar.pup:102', "bolivar.pup:$choice") -Actions @(
    "waitglobal bolivarphase $phase 60000", 'expectflat flat 0', "expectglobal lookahead $look", 'expectglobal playerturn 1','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-checkers-$Difficulty-ready-native.log"
if ($Difficulty -eq 'easy') { Copy-Item $ready "$root/build/save-dust-checkers-ready.dfsv" }
