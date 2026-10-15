# SPDX-License-Identifier: GPL-3.0-or-later
# Leave the hotel while the room is prepared, return, and collect its key.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-hotel-ring.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-hotel-key.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('fear.pup:101') -ClickMovies @('jacknote.mov') -Actions @(
    'expectset hotlower','expectglobal phase 5','key rightarrow','key rightarrow',
    'expectview west','click 260 160','expectowner door hotout','key uparrow',
    'expectset town','expectscene Scene G5','expectglobal phase 6',
    'key rightarrow','key rightarrow','expectview east','click 250 150',
    'expectowner door hotel','key uparrow','expectset hotlower','expectscene Scene A1',
    'expectview east','clickon actor fear','expectowner hrkey stranger',
    'expectglobal phase 7','expectowner ring fear','expectglobal playerdeath ',
    'savecheckpoint','targets'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-hotel-key-native.log"
