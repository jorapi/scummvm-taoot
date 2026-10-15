# SPDX-License-Identifier: GPL-3.0-or-later
# Earn the direct two-victory ring reward from a saved first victory.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-fencing-won.dfsv", [int]$TimeoutSeconds = 420)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/room-route.mjs" full SQHALL View41 View42
if ($LASTEXITCODE) { throw 'Cannot plan return to the squash court' }
$actions = @($route | ForEach-Object { ($_ -split ' # ')[0] }) + @(
    'clickon painting door','key uparrow','expectset squash','expectflat fence 8',
    'expectglobal fencewins 1','expectglobal fencecount 1','savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-fencing-second-ready.dfsv" -TimeoutSeconds $TimeoutSeconds -Choices @('wilfenc1.pup:101') -Actions $actions
Copy-Item "$root/build/boot-full.log" "$root/build/full-fencing-second-ready-native.log"
& "$PSScriptRoot/verify-full-fencing-win.ps1" -RestoreCheckpoint "$root/build/save-full-fencing-second-ready.dfsv" -TracePrefix fencing-second -TimeoutSeconds $TimeoutSeconds
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-fencing-second-won.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal fencewins 2','expectglobal fencecount 2','expectactorowner willie won',
    'expectowner ring frank','expectglobal phase 2','expectset sqhall','expectflat main 1',
    'key rightarrow','key leftarrow','expectview View41'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-fencing-second-ring-restore-native.log"
Write-Output 'Titanic direct ring award after two earned fencing wins and fresh restore PASS'
