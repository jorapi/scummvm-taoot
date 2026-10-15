# SPDX-License-Identifier: GPL-3.0-or-later
# Leave the recovered notebook encounter and enter the authored sinking chapter.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-notebook-recovered.dfsv", [int]$TimeoutSeconds = 480)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/room-route.mjs" full SMSTACK1 View42 View41
if ($LASTEXITCODE) { throw 'Cannot plan smokestack exit' }
$route = $route | ForEach-Object { ($_ -split ' # ')[0] }
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-mission4.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectset smstack3', 'expectview View52', 'expectglobal mission 3', 'expectglobal phase 3',
    'expectowner notebook frank', 'expectowner gaspen zeit',
    'key rightarrow', 'expectview View54', 'key downarrow',
    'expectset smstack1', 'expectview View42', 'expectglobal stacklevel 1'
) + @($route) + @(
    'key uparrow', 'expectglobal mission 4', 'expectglobal phase 0',
    'expectset c73', 'expectview View63', 'expectowner notebook vlad',
    'expectowner painting frank', 'expectowner rubaiyat frank', 'expectglobal playerdeath ',
    'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-mission4-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-mission4.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal mission 4', 'expectglobal phase 0', 'expectset c73', 'expectview View63',
    'expectowner notebook vlad', 'expectowner painting frank', 'expectowner rubaiyat frank',
    'key rightarrow', 'expectset c73', 'expectview View65'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-mission4-restore-native.log"
