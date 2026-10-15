# SPDX-License-Identifier: GPL-3.0-or-later
# Follow the actual mask-guided destination and give the mask to the skeleton.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-mine-puzzle-entry.dfsv", [int]$TimeoutSeconds = 360)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$state = (& python "$PSScriptRoot/read-checkpoint-globals.py" $RestoreCheckpoint standx standy | ConvertFrom-Json)
if ($LASTEXITCODE) { throw 'Cannot read the earned mine destination' }
$r = @(& node "$PSScriptRoot/dust-mine-route.mjs" $state.standx $state.standy)
if ($LASTEXITCODE) { throw 'Cannot plan the repeating mine tunnels' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-mine-puzzle-solved.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectset mine','expectscene Scene C5','expectview north','expectglobal handitem mask','expectactorvalue skeleton 1'
) + $r + @('dragto prop mask actor skeleton','expectglobal minepuzzle done','expectglobal flutepuzzle done',
    'expectglobal snakepuzzle done','expectglobal tbirdpuzzle done','expectglobal phase 1',
    'expectowner mask skeleton','expectset hub','expectflat mainpanel',
    'expectglobal playerdeath ','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-mine-puzzle-solved-native.log"
