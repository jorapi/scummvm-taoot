# SPDX-License-Identifier: GPL-3.0-or-later
# Open Sasha's doll through physical dial rotations, then swap the necklaces.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-sasha-poweron.dfsv", [int]$TimeoutSeconds = 600)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/room-route.mjs" full HALLA View61 View57
if ($LASTEXITCODE) { throw 'Cannot plan Sasha door route' }
$route = @($route | ForEach-Object { ($_ -split ' # ')[0] })
$cabin = & node "$PSScriptRoot/room-route.mjs" full A14 View20 View11
if ($LASTEXITCODE) { throw 'Cannot plan Sasha cabin route' }
$cabin = @($cabin | ForEach-Object { ($_ -split ' # ')[0] })
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-patty-dials.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal neckphase 8', 'expectowner fakeneck frank'
) + $route + @(
    'clickon painting door', 'key uparrow', 'expectset a14', 'expectview View20'
) + $cabin + @(
    'clickon painting patty', 'expectflat patty 1', 'clickon button dial', 'expectflat patty 2',
    'dumpprop dial1', 'dumpprop dial2', 'dumpprop dial3', 'dumpprop dial4', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-patty-dials-native.log"
$log = Get-Content "$root/build/full-patty-dials-native.log" -Raw
$actions = @('expectflat patty 2')
for ($dial = 1; $dial -le 4; $dial++) {
    $match = [regex]::Match($log, "BOOT PROP name=dial$dial deg=(\d+)")
    if (!$match.Success) { throw "Missing initial dial$dial angle" }
    $target = if ($dial -eq 1) { 6 } else { 0 }
    $steps = ([int]$match.Groups[1].Value - $target + 24) % 24
    if ($steps) { $actions += "dragarc prop dial$dial 256 192 16 $steps" }
    $actions += "dumpprop dial$dial"
    $actions += "expectpropdeg dial$dial $target"
}
$actions += @(
    'clickon button back', 'expectflat patty 1', 'clickon button doll1',
    'clickon prop doll', 'clickon prop doll', 'clickon prop doll', 'expectflat patty 3',
    'clickon prop pattyreal', 'expectowner realneck frank', 'clickon prop pattybag',
    'expectflat inven 1', 'clickon prop fakeneck', 'expectglobal handitem fakeneck',
    'clickthen button ok 150 200 3000', 'expectflat patty 3', 'expectowner fakeneck vlad',
    'expectowner realneck frank', 'savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-patty-dials.dfsv" -Checkpoint "$root/build/save-full-necklace-swapped.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions $actions
Copy-Item "$root/build/boot-full.log" "$root/build/full-necklace-swapped-native.log"
