# SPDX-License-Identifier: GPL-3.0-or-later
# Investigate Haderlitz's death using dialogue, doors and the authored clue close-up.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-mission3.dfsv", [int]$TimeoutSeconds = 480)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-BathRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
$checkpoint = "$root/build/save-full-rubiclue.dfsv"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-bath-permission.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal mission 3', 'expectglobal phase 0',
    'clickon prop map', 'key f', 'expectflat Map 7', 'clickon button Button44',
    'expectset turkstrs', 'expectview View16',
    'clickon actor smeth', 'expectglobal smethphase 1',
    'clickon painting door', 'expectglobal morrowphase 1',
    # Conversation returns Morrow to his original star before the door is usable.
    'waitactorwalk morrow 0', 'savecheckpoint'
) -Choices @(
    'smeth1.pup:101', 'smeth1.pup:104', 'smeth1.pup:102', 'smeth1.pup:103',
    'morrow1.pup:101', 'morrow1.pup:102', 'morrow1.pup:101', 'morrow1.pup:103', 'morrow1.pup:102'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-bath-permission-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-bath-permission.dfsv" -Checkpoint $checkpoint -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal morrowphase 1', 'expectview View16',
    'clickon painting door', 'key uparrow', 'expectset turk', 'expectview View116'
) + @(Get-BathRoute TURK View116 View120) + @(
    'clickon painting door', 'key uparrow', 'expectset ehall', 'expectview enter'
) + @(Get-BathRoute EHALL enter View24) + @(
    'clickon painting door', 'key uparrow', 'expectset ebath', 'expectview View19',
    'clickon painting bath', 'expectflat cuff 1',
    'clickon prop rubclueclue', 'expectpropview rubclueclue big',
    'clickon prop rubclueclue', 'expectowner rubiclue frank',
    'click 457 351', 'expectflat main 1',
    'expectglobal mission 3', 'expectglobal phase 0', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-rubiclue-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $checkpoint -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectset ebath', 'expectview View19', 'expectflat main 1',
    'expectglobal mission 3', 'expectglobal phase 0', 'expectglobal morrowphase 1',
    'expectglobal smethphase 1', 'expectowner rubiclue frank', 'expectowner gaspen frank',
    'key rightarrow', 'expectview View16'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-rubiclue-restore-native.log"
