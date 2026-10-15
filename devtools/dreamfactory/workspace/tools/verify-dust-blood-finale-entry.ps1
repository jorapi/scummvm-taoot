# SPDX-License-Identifier: GPL-3.0-or-later
# Touch the revealed chest and follow Bloodstone's authored final confrontation.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-chest-revealed.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-blood-finale-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'blood.pup:101','blood.pup:101'
) -Actions @('expectglobal phase 3','expectset hub','clickon prop chest','expectglobal phase 5',
    'expectset tbird','expectscene Scene B2','expectview south','expectowner tbird stranger',
    'expectowner gun none','expectglobal playerdeath ','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-blood-finale-entry-native.log"
