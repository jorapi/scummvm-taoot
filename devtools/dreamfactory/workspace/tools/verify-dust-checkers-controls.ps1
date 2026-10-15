# SPDX-License-Identifier: GPL-3.0-or-later
# Check the portrait, real save-slot rollback and voluntary exit from a live board.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-checkers-ready.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$state = & python "$PSScriptRoot/read-checkpoint-globals.py" $RestoreCheckpoint mainboard | ConvertFrom-Json
if ($LASTEXITCODE) { throw 'Cannot inspect the earned initial board' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-checkers-slot-rollback.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectflat flat 0','clickon button avatar','expectpropview avatar think','expectflat flat 0',
    'saveslot 99','dragpoint prop me1 182 147','loadslot 99',
    'expectglobal playerturn 1','expectglobal gameover 0','expectpropview avatar think','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-checkers-slot-rollback-native.log"
# The action protocol trims commands; compare the actual saved string here,
# preserving the board's authored trailing space as well as all 64 cells.
$restored = & python "$PSScriptRoot/read-checkpoint-globals.py" "$root/build/save-dust-checkers-slot-rollback.dfsv" mainboard | ConvertFrom-Json
if ($LASTEXITCODE -or $restored.mainboard -cne $state.mainboard) { throw 'Save-slot rollback changed the initial board' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-checkers-slot-rollback.dfsv" -Checkpoint "$root/build/save-dust-checkers-controls-return.dfsv" -TimeoutSeconds $TimeoutSeconds -Choices @('bolivar.pup:104') -Actions @(
    'clickon button exit',
    'waitglobal bolivarphase 2 60000','expectglobal score 0','expectset store','expectflat mainpanel',
    'expectview north','key rightarrow','expectview east','key leftarrow','expectview north','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-checkers-controls-native.log"
Write-Output 'Dust checkers portrait, native save-slot rollback and voluntary exit PASS'
