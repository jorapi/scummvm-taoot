# SPDX-License-Identifier: GPL-3.0-or-later
# Repair the turbine again to open the engineer's mission-three door.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-smokestack-lead.dfsv", [int]$TimeoutSeconds = 360)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-Turbine2Route([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-turbine2-ready.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal mission 3', 'expectglobal phase 1',
    'clickon prop map', 'key f', 'expectflat Map 7', 'clickon button Button22',
    'expectset stair2c', 'expectview View28', 'clickon painting door',
    'key uparrow', 'expectset hallf2c', 'expectview View70'
) + @(Get-Turbine2Route HALLF2C View70 View13) + @(
    'clickon painting door', 'key uparrow', 'expectset turb', 'expectview View89'
) + @(Get-Turbine2Route TURB View89 View74) + @(
    'clickon painting door', 'key uparrow', 'expectset control', 'expectview View13'
) + @(Get-Turbine2Route CONTROL View13 View24) + @(
    'clickon painting controls', 'expectflat turbine 1', 'expectactorowner csea helping', 'savecheckpoint'
)) -Choices @('csea1.pup:101')
Copy-Item "$root/build/boot-full.log" "$root/build/full-turbine2-ready-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-turbine2-ready.dfsv" -Checkpoint "$root/build/save-full-turbine2-pending.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'dragarc prop valve3 140 297 16 24', 'expectpropdeg valve3 0',
    'dragarc prop valve3 140 297 -16 7', 'expectpropdeg valve3 7', 'expectglobal valve3 36',
    'dragarc prop pump1 372 309 -16 24', 'expectpropdeg pump1 0',
    'dragarc prop pump1 372 309 16 5', 'expectpropdeg pump1 15', 'expectglobal pump1 22',
    'dragpoint prop slider 192 280', 'expectpropdeg slider 7', 'expectglobal coal 68',
    'holduntil prop valve3 electlag 74', 'expectglobalge electlag 74',
    'clickon button exit', 'expectflat main 1', 'expectactorowner csea thanks2',
    'expectglobal mission 3', 'expectglobal phase 1', 'expectset control', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-turbine2-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-turbine2-pending.dfsv" -Checkpoint "$root/build/save-full-turbine2.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal lockevents 1', 'waitglobal lockevents 0 30000',
    'expectactorowner csea thanks2', 'expectset control', 'expectview View24',
    'expectglobal mission 3', 'expectglobal phase 1', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-turbine2-restore-native.log"
