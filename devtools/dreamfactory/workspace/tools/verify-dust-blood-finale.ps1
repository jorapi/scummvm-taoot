# SPDX-License-Identifier: GPL-3.0-or-later
# The alternate Thunderbird alignment defeats Bloodstone and reaches day five.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-blood-finale-entry.dfsv", [int]$TimeoutSeconds = 420)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-blood-final-tumblers.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions @(
    'expectglobal phase 5','expectset tbird','expectview south','expectglobal handitem tbird',
    'key rightarrow','key rightarrow','expectview north','click 260 150','expectflat tumble0',
    'dragpoint prop tbird 250 100','expectflat tumble2','expectowner tbird temple',
    'expectpropdeg tumbler1 0','expectpropdeg tumbler2 0','expectpropdeg tumbler3 0',
    'expectpropdeg tumbler4 0','expectpropdeg tumbler5 0','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-blood-final-tumblers-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-blood-final-tumblers.dfsv" -Checkpoint "$root/build/save-dust-blood-final-combination.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions @(
    'click 115 120','click 115 120','expectpropdeg tumbler1 8',
    'click 186 120','click 186 120','expectpropdeg tumbler2 8','expectpropdeg tumbler3 0',
    'click 326 70','expectpropdeg tumbler4 20','click 395 70','click 395 70','expectpropdeg tumbler5 16',
    'expectglobal phase 5','expectglobal playerdeath ','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-blood-final-combination-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-blood-final-combination.dfsv" -Checkpoint "$root/build/save-dust-day5-morning.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions @(
    'clickon button quit','expectglobal day 5','expectglobal clock 1','expectglobal phase 0',
    'expectset town','expectscene Scene G4','expectview south','expectowner chest stranger',
    'expectglobal playerdeath ','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-blood-defeated-native.log"
foreach ($movie in @('bloodies.mov','d4nd5m.mov')) {
    if ((Get-Content "$root/build/dust-blood-defeated-native.log" -Raw) -notmatch ('MOVIE PLAY '+[regex]::Escape($movie))) { throw "Missing final confrontation movie: $movie" }
}
