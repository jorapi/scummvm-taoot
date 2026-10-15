# SPDX-License-Identifier: GPL-3.0-or-later
# Physical upper/lower tumbler clicks produce the authored 8,8,4,20,12 alignment.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-tbird-tumblers.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-tbird-puzzle-solved.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions @(
    'expectflat tumble2','click 115 120','expectpropdeg tumbler1 4','click 115 120','expectpropdeg tumbler1 8',
    'click 186 120','click 186 120','expectpropdeg tumbler2 8',
    'click 256 120','expectpropdeg tumbler3 4','click 326 70','expectpropdeg tumbler4 20',
    'click 395 120','click 395 120','click 395 120','expectpropdeg tumbler5 12',
    'clickon button quit','expectglobal tbirdpuzzle done','expectglobal snakepuzzle done','expectglobal flutepuzzle done',
    'expectset hub','expectflat mainpanel','expectglobal day 4','expectglobal clock 3',
    'expectglobal phase 0','expectglobal playerdeath ','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-tbird-puzzle-solved-native.log"
