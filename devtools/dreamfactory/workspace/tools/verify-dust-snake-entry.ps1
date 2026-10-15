# SPDX-License-Identifier: GPL-3.0-or-later
# Align the sundial and open the seven coupled snake rotors through native input.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-sundial-after-flute.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$r = @(& node "$PSScriptRoot/dust-route.mjs" HUB D5 north A4 west)
if ($LASTEXITCODE) { throw 'Cannot plan snake portal approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-snake-portal-dark.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectglobal flutepuzzle done','expectpropdeg largedial 0','expectpropdeg meddial 0','expectpropdeg smalldial 8',
    'dragarc prop largedial 256 132 16 8','expectpropdeg largedial 8',
    'dragarc prop meddial 256 132 -16 4','expectpropdeg meddial 12',
    'dragarc prop smalldial 256 132 -16 4','expectpropdeg smalldial 4',
    'clickon button exit','expectglobal nextroom snake'
) + $r + @('key uparrow','expectset hub','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-snake-portal-dark-native.log"
$state = (& python "$PSScriptRoot/read-checkpoint-globals.py" "$root/build/save-dust-snake-portal-dark.dfsv" blackout | ConvertFrom-Json)
if ($LASTEXITCODE -or $state.blackout -lt 1 -or $state.blackout -gt 6) { throw 'Unexpected portal darkness counter' }
$a = @()
1..($state.blackout + 1) | ForEach-Object { $a += 'key leftarrow' }
$r = @(& node "$PSScriptRoot/dust-route.mjs" SNAKE B4 north B3 south)
if ($LASTEXITCODE) { throw 'Cannot plan snake puzzle approach' }
$a += @('expectset snake','expectscene Scene B4','expectview north') + $r + @('click 350 160','expectflat flat 0')
1..7 | ForEach-Object { $a += "dumpprop stop$_" }
$a += @('expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-snake-portal-dark.dfsv" -Checkpoint "$root/build/save-dust-snake-puzzle-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-snake-puzzle-entry-native.log"
