# SPDX-License-Identifier: GPL-3.0-or-later
# Recover the pledged ring from Marie through her authored conversation.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-ring-marie.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G5 west J9 east)
if ($LASTEXITCODE) { throw 'Cannot plan Marie approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-ring-returned.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'marie.pup:111','marie.pup:104'
) -Actions (@('expectowner ring marie','expectowner history none','key rightarrow','key rightarrow',
    'expectview west','click 250 150','expectowner door hotout','key uparrow','expectset town'
) + $town + @('clickon actor marie','expectowner ring stranger','expectglobal mariephase 1',
    'expectglobal jonesringstory 1','expectowner pages stranger','expectglobal day 3','expectglobal clock 1',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-ring-returned-native.log"
