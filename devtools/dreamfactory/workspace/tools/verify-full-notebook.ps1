# SPDX-License-Identifier: GPL-3.0-or-later
# Climb the naturally chosen smokestack maze and retain the notebook using the gas pen.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-vlad-defeated.dfsv", [int]$TimeoutSeconds = 480)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-smokestack-entry.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectset engine', 'expectview View119', 'expectglobal mission 3', 'expectglobal phase 2',
    'expectactorowner vlad lostfight', 'key leftarrow', 'key leftarrow', 'expectview View120',
    'key uparrow', 'expectset smstack1', 'expectview View40', 'expectglobal stacklevel 1', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-smokestack-entry-native.log"
$match = [regex]::Match((Get-Content "$root/build/boot-full.log" -Raw), 'SCRIPT MESSAGE maze number([1-4])')
if (!$match.Success) { throw 'Native entry did not identify its authored maze layout' }
$maze = [int]$match.Groups[1].Value
$route = & node "$PSScriptRoot/smokestack-route.mjs" $maze
if ($LASTEXITCODE) { throw "Cannot plan smokestack maze $maze" }
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-smokestack-entry.dfsv" -Checkpoint "$root/build/save-full-notebook-top.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectset smstack1', 'expectview View40', "expectglobal mazenumber $maze",
    'expectglobal mission 3', 'expectglobal phase 2'
) + @($route) + @(
    'expectset smstack3', 'expectview View53', 'expectowner notebook none', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-smokestack-maze-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-notebook-top.dfsv" -Checkpoint "$root/build/save-full-notebook-recovered.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectset smstack3', 'expectview View53', 'expectglobal mission 3', 'expectglobal phase 2',
    'clickon prop notebook', 'expectglobal mission 3', 'expectglobal phase 3',
    'expectowner notebook frank', 'expectowner gaspen zeit', 'savecheckpoint'
) -Choices @(
    'zeit1.pup:101', 'zeit1.pup:101', 'zeit1.pup:101', 'zeit1.pup:101',
    'zeit1.pup:55555', 'zeit1.pup:55555'
) -ModalClicks @('inven 1:prop:gaspen', 'inven 1:button:ok') -ClickMovies @('berg.mov')
Copy-Item "$root/build/boot-full.log" "$root/build/full-notebook-recovered-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-notebook-recovered.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectset smstack3', 'expectview View52', 'expectglobal mission 3', 'expectglobal phase 3',
    'expectowner notebook frank', 'expectowner gaspen zeit', 'expectglobal lockevents 0',
    'key rightarrow', 'expectset smstack3', 'expectview View54'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-notebook-restore-native.log"
