# SPDX-License-Identifier: GPL-3.0-or-later
# Return from earned demo progress and replay through authored menu callbacks.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-demo-photograph.dfsv", [switch]$SkipOpening, [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions demo -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-demo-replay-menu.dfsv" -TimeoutSeconds $TimeoutSeconds -DialogAnswers @('yes','no') -Actions @(
    'clickon prop life','expectflat ctl 1','click 130 160','expectset none','expectflat demo 1','savecheckpoint')
Copy-Item "$root/build/boot-demo.log" "$root/build/demo-replay-menu-native.log"
$out = & node "$PSScriptRoot/room-route.mjs" demo C71 View63 View60
if ($LASTEXITCODE) { throw 'Cannot plan replay cabin turn' }
$back = & node "$PSScriptRoot/room-route.mjs" demo C71 View60 View63
if ($LASTEXITCODE) { throw 'Cannot plan replay return turn' }
$skips = if ($SkipOpening) { @('flyby.mov') } else { @() }
& "$PSScriptRoot/trace-boot.ps1" -Editions demo -RestoreCheckpoint "$root/build/save-demo-replay-menu.dfsv" -Checkpoint "$root/build/save-demo-restarted.dfsv" -TimeoutSeconds $TimeoutSeconds -ClickMovies @('date.mov') -SkipMovies $skips -Actions (@(
    'clickon button demo','waitset c71 60000','expectsetting roomvisible 1','expectglobal lockevents 0',
    'expectview View63','expectflat main 1','expectglobal phase 0','expectglobal handitem ',
    'expectowner photo1 none','expectowner bag none','expectowner watch none'
) + @($out | ForEach-Object { ($_ -split ' # ')[0] }) + @('expectview View60') + @($back | ForEach-Object { ($_ -split ' # ')[0] }) + @('expectview View63','savecheckpoint'))
Copy-Item "$root/build/boot-demo.log" "$root/build/demo-restart-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions demo -RestoreCheckpoint "$root/build/save-demo-restarted.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectsetting roomvisible 1','expectglobal phase 0','expectowner photo1 none','expectview View63','key uparrow','expectview View54')
Copy-Item "$root/build/boot-demo.log" "$root/build/demo-restart-restore-native.log"
Write-Output 'Supplied demo replay reset, visible cabin, navigation and fresh restore PASS'
