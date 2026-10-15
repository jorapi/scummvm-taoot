# SPDX-License-Identifier: GPL-3.0-or-later
# Give Fearwitt the pharmacy handkerchief and receive its authored chit state.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-handkerchief.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exit = & node "$PSScriptRoot/dust-route.mjs" APOTH A2 south C2 east
if ($LASTEXITCODE) { throw 'Cannot plan pharmacy exit' }
$hotel = & node "$PSScriptRoot/dust-route.mjs" TOWN G9 west G5 east
if ($LASTEXITCODE) { throw 'Cannot plan hotel return' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-handkerchief-chit.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal day 2','expectglobal clock 2','expectowner hankerchief stranger','expectpropdeg hankerchief 0'
) + @($exit) + @(
    'click 250 150','expectowner door pharm','key uparrow','expectset town',
    'expectscene Scene G9','expectview west'
) + @($hotel) + @(
    'click 250 150','expectowner door hotel','key uparrow','expectset hotlower',
    'expectscene Scene A1','expectview east','expectglobal handitem hankerchief',
    'dragto prop hankerchief actor fear','expectowner hankerchief stranger',
    'expectpropdeg hankerchief 1','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-handkerchief-chit-native.log"
