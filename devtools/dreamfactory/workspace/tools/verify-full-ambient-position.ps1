# SPDX-License-Identifier: GPL-3.0-or-later
# Walk/turn in real ship data; mixer observers never alter sound or game state.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-turbine2.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Route([string]$Room, [string]$From, [string]$To) {
    $actions = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    @($actions | ForEach-Object { ($_ -split ' # ')[0] })
}
$actions = @('expectset control','expectview View24') + @(Route CONTROL View24 View14) + @(
    'clickon painting door','key uparrow','expectset turb','expectview View75'
) + @(Route TURB View75 View68) + @(
    'idle 3','expectambient motor 1 148 -54',
    'key rightarrow','expectview View70','idle 3','expectambient motor 1 148 74',
    'savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-ambient-position.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions $actions
Copy-Item "$root/build/boot-full.log" "$root/build/full-ambient-position-native.log"
$actions = @('expectset turb','expectview View70','expectambient motor 1 148 74',
    'key leftarrow','expectview View68','idle 3','expectambient motor 1 148 -54'
) + @(Route TURB View68 View17) + @(
    'idle 3','expectambient motor 1 187 24'
) + @(Route TURB View17 View73) + @(
    'idle 3','expectambient motor 0 0 0','savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-ambient-position.dfsv" -Checkpoint "$root/build/save-full-ambient-radius-exit.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions $actions
Copy-Item "$root/build/boot-full.log" "$root/build/full-ambient-position-restore-native.log"
Write-Output 'Titanic authored motor ambience turn, saved live position, walking volume and radius exit PASS'
