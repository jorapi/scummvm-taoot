# SPDX-License-Identifier: GPL-3.0-or-later
# Return the necklace branch to Claris's ring-mission corridor entry point.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-necklace-recovered.dfsv", [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/room-route.mjs" full RECEPT1C View27 View25
if ($LASTEXITCODE) { throw 'Cannot plan reception route' }
$route = @($route | ForEach-Object { ($_ -split ' # ')[0] })
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-necklace-ring-ready.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 2', 'expectglobal phase 2', 'expectglobal neckphase 9',
    'expectowner realneck frank', 'expectowner painting frank', 'expectowner rubaiyat frank', 'expectowner ring frank',
    'clickon prop map', 'key d', 'expectflat Map 5', 'clickon button Button42',
    'expectset recept1c', 'expectview View27'
) + $route + @('expectview View25','savecheckpoint'))
Copy-Item "$root/build/boot-full.log" "$root/build/full-necklace-ring-ready-native.log"
