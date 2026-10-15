# SPDX-License-Identifier: GPL-3.0-or-later
# Walk from Claris's shawl to Vlad and trade it for the stolen notebook.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-sinking-shawl.dfsv", [int]$TimeoutSeconds = 600)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-SinkingRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To 1
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-sinking-boatdeck.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectset lnghall', 'expectview View12', 'expectowner shawl frank',
    'key uparrow', 'expectset lounge1c', 'expectview View16'
) + @(Get-SinkingRoute LOUNGE1C View16 View36) + @(
    'clickon painting door', 'key uparrow', 'expectset lnghall', 'expectview View13'
) + @(Get-SinkingRoute LNGHALL View13 View17) + @(
    'clickon painting door', 'key uparrow', 'expectset gstair2', 'expectview View34'
) + @(Get-SinkingRoute GSTAIR2 View34 View68) + @(
    'expectset gstair1', 'expectview View29'
) + @(Get-SinkingRoute GSTAIR1 View29 View43) + @(
    'clickon painting door', 'key uparrow', 'expectset veststbd', 'expectview View18',
    'clickon painting door', 'key uparrow', 'expectset deckbd2', 'expectview View114', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-boatdeck-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-sinking-boatdeck.dfsv" -Checkpoint "$root/build/save-full-sinking-stairs-top.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    Get-SinkingRoute DECKBD2 View114 View266
) + @(
    'clickon painting door', 'key uparrow', 'expectset stair2c', 'expectview View64',
    'expectglobal savedeck bd', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-stairs-top-native.log"
$stairs = & node "$PSScriptRoot/stair2c-route.mjs" bd View64 landing View28
if ($LASTEXITCODE) { throw 'Cannot plan the sinking staircase descent' }
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-sinking-stairs-top.dfsv" -Checkpoint "$root/build/save-full-sinking-stairs-bottom.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@($stairs) + @(
    'expectview View28', 'clickon painting door', 'key uparrow',
    'expectset hallf2c', 'expectview View70', 'savecheckpoint'
)) -Choices @('csea2.pup:101')
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-stairs-bottom-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-sinking-stairs-bottom.dfsv" -Checkpoint "$root/build/save-full-sinking-notebook.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    Get-SinkingRoute HALLF2C View70 View13
) + @(
    'clickon painting door', 'key uparrow', 'expectset turb', 'expectview View89'
) + @(Get-SinkingRoute TURB View89 View48) + @(
    'waitactorwalk vlad 0', 'clickon actor vlad', 'expectowner notebook frank',
    'expectowner shawl vlad', 'expectglobal mission 4', 'expectowner painting frank',
    'expectowner rubaiyat frank', 'savecheckpoint'
)) -Choices @('vlad2.pup:101','vlad2.pup:101','vlad2.pup:101','vlad2.pup:101','vlad2.pup:101','vlad2.pup:102')
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-notebook-native.log"
