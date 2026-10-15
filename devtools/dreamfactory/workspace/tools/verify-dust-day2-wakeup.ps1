# SPDX-License-Identifier: GPL-3.0-or-later
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day2-morning.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" HOTROOM A1 east B1 east
if ($LASTEXITCODE) { throw 'Cannot plan room exit' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day2-wakeup.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('jones.pup:101') -Actions (@(
    'expectglobal day 2','expectglobal clock 1','expectglobal phase 0'
) + @($route) + @(
    'click 250 150','expectowner door inside','key uparrow','expectset hotupper',
    'idle 60','expectglobal phase 1','expectglobal lockevents 0',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-day2-wakeup-native.log"
