# SPDX-License-Identifier: GPL-3.0-or-later
# Hear the shaman after all four trials, then reveal the chest through the sundial.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-mine-puzzle-solved.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$state = (& python "$PSScriptRoot/read-checkpoint-globals.py" $RestoreCheckpoint blackout | ConvertFrom-Json)
if ($LASTEXITCODE -or $state.blackout -lt 1 -or $state.blackout -gt 6) { throw 'Unexpected hub darkness counter' }
$a = @('expectset hub','expectglobal phase 1','expectglobal minepuzzle done')
1..($state.blackout + 1) | ForEach-Object { $a += 'key leftarrow' }
$r = @(& node "$PSScriptRoot/dust-route.mjs" HUB D7 north C4 east)
if ($LASTEXITCODE) { throw 'Cannot plan shaman approach' }
$a += @('expectglobal blackout -1') + $r + @('clickon actor shaman','expectglobal phase 2','expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-shaman.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @('shaman.pup:101','shaman.pup:101') -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-shaman-native.log"
$r = @(& node "$PSScriptRoot/dust-route.mjs" HUB C4 east C4 east)
if ($LASTEXITCODE) { throw 'Cannot plan sundial approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-shaman.dfsv" -Checkpoint "$root/build/save-dust-chest-revealed.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions ($r + @(
    'click 250 160','expectflat flat 0','expectpropdeg largedial 4','expectpropdeg meddial 8','expectpropdeg smalldial 12',
    'dragarc prop largedial 256 132 16 8','expectpropdeg largedial 12',
    'dragarc prop meddial 256 132 -16 4','expectpropdeg meddial 4','clickon button exit',
    'expectglobal phase 3','expectflat mainpanel','expectset hub','dumpprop chest','expectglobal playerdeath ','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-chest-revealed-native.log"
