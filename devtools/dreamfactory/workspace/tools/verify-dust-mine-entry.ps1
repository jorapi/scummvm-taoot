# SPDX-License-Identifier: GPL-3.0-or-later
# Enter the mine holding the earned mask, which fixes the skeleton's destination.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-sundial-after-tbird.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$r = @(& node "$PSScriptRoot/dust-route.mjs" HUB D5 north D1 north)
if ($LASTEXITCODE) { throw 'Cannot plan mine portal approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-mine-portal-dark.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectglobal flutepuzzle done','expectglobal snakepuzzle done','expectglobal tbirdpuzzle done',
    'expectpropdeg largedial 12','expectpropdeg meddial 4','expectpropdeg smalldial 8',
    'dragarc prop largedial 256 132 -16 8','expectpropdeg largedial 4',
    'dragarc prop meddial 256 132 16 4','expectpropdeg meddial 8',
    'dragarc prop smalldial 256 132 16 4','expectpropdeg smalldial 12',
    'clickon button exit','expectglobal nextroom mine','clickon prop avatar','expectflat avatar',
    'clickon prop mask','clickon button ok','expectglobal handitem mask'
) + $r + @('key uparrow','expectset hub','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-mine-portal-dark-native.log"
$state = (& python "$PSScriptRoot/read-checkpoint-globals.py" "$root/build/save-dust-mine-portal-dark.dfsv" blackout | ConvertFrom-Json)
if ($LASTEXITCODE -or $state.blackout -lt 1 -or $state.blackout -gt 6) { throw 'Unexpected portal darkness counter' }
$a = @()
1..($state.blackout + 1) | ForEach-Object { $a += 'key leftarrow' }
$a += @('expectset mine','expectscene Scene C5','expectview north','expectglobal handitem mask',
    'idle 100','expectactorvalue skeleton 1','dumpactor skeleton','expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-mine-portal-dark.dfsv" -Checkpoint "$root/build/save-dust-mine-puzzle-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-mine-puzzle-entry-native.log"
if ((Get-Content "$root/build/dust-mine-puzzle-entry-native.log" -Raw) -notmatch 'BOOT ACTOR name=skeleton .*visible=1 ') { throw 'Skeleton has not finished its offscreen placement' }
