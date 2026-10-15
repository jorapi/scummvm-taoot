# SPDX-License-Identifier: GPL-3.0-or-later
# Select the Thunderbird chamber and place the earned artifact in its podium.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-sundial-after-snake.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$r = @(& node "$PSScriptRoot/dust-route.mjs" HUB D5 north D7 south)
if ($LASTEXITCODE) { throw 'Cannot plan Thunderbird portal approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-tbird-portal-dark.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectglobal flutepuzzle done','expectglobal snakepuzzle done',
    'expectpropdeg largedial 8','expectpropdeg meddial 12','expectpropdeg smalldial 4',
    'dragarc prop largedial 256 132 16 4','expectpropdeg largedial 12',
    'dragarc prop meddial 256 132 16 8','expectpropdeg meddial 4',
    'dragarc prop smalldial 256 132 16 4','expectpropdeg smalldial 8',
    'clickon button exit','expectglobal nextroom tbird'
) + $r + @('key uparrow','expectset hub','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-tbird-portal-dark-native.log"
$state = (& python "$PSScriptRoot/read-checkpoint-globals.py" "$root/build/save-dust-tbird-portal-dark.dfsv" blackout | ConvertFrom-Json)
if ($LASTEXITCODE -or $state.blackout -lt 1 -or $state.blackout -gt 6) { throw 'Unexpected portal darkness counter' }
$a = @()
1..($state.blackout + 1) | ForEach-Object { $a += 'key leftarrow' }
$r = @(& node "$PSScriptRoot/dust-route.mjs" TBIRD B4 north B2 north)
if ($LASTEXITCODE) { throw 'Cannot plan podium approach' }
$a += @('expectset tbird','expectscene Scene B4','expectview north') + $r + @('click 260 150','expectflat tumble0','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-tbird-portal-dark.dfsv" -Checkpoint "$root/build/save-dust-tbird-podium.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-tbird-podium-native.log"
$a = @('clickon prop avatar','expectflat tumble0','expectglobal handitem tbird',
    'dragpoint prop tbird 250 100','expectowner tbird temple','expectflat tumble2')
1..5 | ForEach-Object { $a += "expectpropdeg tumbler$_ 0" }
$a += @('expectglobal playerdeath ','targets','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-tbird-podium.dfsv" -Checkpoint "$root/build/save-dust-tbird-tumblers.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -ModalClicks @(
    'avatar:prop:tbird','avatar:button:ok'
) -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-tbird-tumblers-native.log"
