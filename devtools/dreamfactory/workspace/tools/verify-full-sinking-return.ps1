# SPDX-License-Identifier: GPL-3.0-or-later
# Return the recovered evidence to the boat deck and exercise the first timed sinking phase.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-sinking-clock.dfsv", [int]$TimeoutSeconds = 420)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-ReturnRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To 1
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
$stairs = & node "$PSScriptRoot/stair2c-route.mjs" f View29 landing View64
if ($LASTEXITCODE) { throw 'Cannot plan the staircase ascent' }
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-sinking-stairs-return.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectset turb', 'expectview View48', 'expectowner notebook frank', 'expectowner shawl vlad'
) + @(Get-ReturnRoute TURB View48 View90) + @(
    'clickon painting door', 'key uparrow', 'expectset hallf2c', 'expectview View14'
) + @(Get-ReturnRoute HALLF2C View14 View69) + @(
    'clickon painting door', 'key uparrow', 'expectset stair2c', 'expectview View29', 'expectglobal savedeck f'
) + @($stairs) + @('expectview View64', 'savecheckpoint'))
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-stairs-return-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-sinking-stairs-return.dfsv" -Checkpoint "$root/build/save-full-sinking-return.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'clickon painting door', 'key uparrow', 'expectset deckbd2', 'expectview View250',
    'idle 25', 'expectglobalge clock 1300',
    'expectowner notebook frank', 'expectowner painting frank', 'expectowner rubaiyat frank', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-return-native.log"
$clock = & python "$PSScriptRoot/read-checkpoint-globals.py" "$root/build/save-full-sinking-return.dfsv" phase
if ($LASTEXITCODE) { throw 'Cannot inspect the boat-deck phase' }
$phase = ($clock | ConvertFrom-Json).phase
if ($phase -notin @(0,1)) { throw "Unexpected boat-deck phase: $phase" }
$phaseActions = if ($phase -eq 0) { @('expectglobal phase 0','idle 3000') } else { @('expectglobal phase 1') }
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-sinking-return.dfsv" -Checkpoint "$root/build/save-full-sinking-phase1.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@($phaseActions) + @(
    # Arrival may already have played sink1.mov; otherwise ordinary idle waits for it.
    'expectglobal mission 4', 'expectglobal phase 1',
    'expectset deckbd2', 'expectowner notebook frank', 'expectowner painting frank',
    'expectowner rubaiyat frank', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-phase1-native.log"
