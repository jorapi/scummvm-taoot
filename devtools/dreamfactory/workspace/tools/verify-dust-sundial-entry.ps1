# SPDX-License-Identifier: GPL-3.0-or-later
# Follow the hub's authored darkness counter and open its sundial.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-hub-entry.dfsv", [string]$Checkpoint = "$PSScriptRoot/../build/save-dust-sundial-entry.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$state = (& python "$PSScriptRoot/read-checkpoint-globals.py" $RestoreCheckpoint blackout | ConvertFrom-Json)
if ($LASTEXITCODE -or $state.blackout -lt 1 -or $state.blackout -gt 6) { throw 'Unexpected initial darkness counter' }
$a = @('expectset hub','expectscene Scene D7','expectview north')
1..($state.blackout + 1) | ForEach-Object { $a += 'key leftarrow' }
$r = @(& node "$PSScriptRoot/dust-route.mjs" HUB D7 north D5 north)
if ($LASTEXITCODE) { throw 'Cannot plan sundial approach' }
$a += @('expectglobal blackout -1','expectview north') + $r + @('click 250 160','expectflat flat 0',
    'dumpprop largedial','dumpprop meddial','dumpprop smalldial','expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint $Checkpoint -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-sundial-entry-native.log"
