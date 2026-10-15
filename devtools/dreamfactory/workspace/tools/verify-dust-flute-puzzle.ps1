# SPDX-License-Identifier: GPL-3.0-or-later
# Align the sundial, cross the dark portal, place the flute and play its melody.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-sundial-entry.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$r = @(& node "$PSScriptRoot/dust-route.mjs" HUB D5 north G4 east)
if ($LASTEXITCODE) { throw 'Cannot plan flute portal approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-flute-portal-dark.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectpropdeg largedial 0','expectpropdeg meddial 0','expectpropdeg smalldial 0',
    'dragarc prop smalldial 256 132 16 8','expectpropdeg smalldial 8','clickon button exit',
    'expectglobal nextroom flute'
) + $r + @('key uparrow','expectset hub','expectglobal nextroom flute','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-flute-portal-dark-native.log"
$state = (& python "$PSScriptRoot/read-checkpoint-globals.py" "$root/build/save-dust-flute-portal-dark.dfsv" blackout | ConvertFrom-Json)
if ($LASTEXITCODE -or $state.blackout -lt 1 -or $state.blackout -gt 6) { throw 'Unexpected portal darkness counter' }
$a = @()
1..($state.blackout + 1) | ForEach-Object { $a += 'key leftarrow' }
$a += @('expectset flute','expectscene Scene B4','expectview north','click 424 230',
    'expectflat flat 0','expectowner flute stranger','expectglobal flutenum 1','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-flute-portal-dark.dfsv" -Checkpoint "$root/build/save-dust-flute-puzzle-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-flute-puzzle-entry-native.log"
$a = @('clickon prop avatar','expectflat flat 0','expectglobal handitem flute',
    'dragpoint prop flute 250 100','expectflat flat 1','expectowner flute temple')
1..5 | ForEach-Object { $a += "clickon button but $_" }
$a += @('expectglobal flutepuzzle done','expectset hub','expectflat mainpanel',
    'expectglobal day 4','expectglobal clock 3','expectglobal phase 0','expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-flute-puzzle-entry.dfsv" -Checkpoint "$root/build/save-dust-flute-puzzle-solved.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -ModalClicks @(
    'avatar:prop:flute','avatar:button:ok'
) -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-flute-puzzle-solved-native.log"
