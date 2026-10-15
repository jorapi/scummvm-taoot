# SPDX-License-Identifier: GPL-3.0-or-later
# Verify the authored late-arrival branch after spending the painting deadline on the necklace quest.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-best-necklace-first-cargo-star.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-CrateRoute([string]$Room,[string]$From,[string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-painting-stolen.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -MovieClicks @('lightoff.mov:switch.SE','lightoff.mov:#1') -Actions (@(
    'expectglobal mission 2','expectglobal phase 0','expectowner painting none','expectowner realneck frank'
) + @(Get-CrateRoute CARGO View18 View16) + @(
    'clickon painting door','key uparrow','expectset bind','expectview View15'
) + @(Get-CrateRoute BIND View15 View16) + @(
    'clickon painting car','expectglobal carlights 1','expectset binl','expectview View16'
) + @(Get-CrateRoute BINL View16 View17) + @(
    'clickon painting crate','expectflat cargo 1','expectowner painting hack',
    'expectglobal mission 2','expectglobal phase 1','expectowner carkeys xxxfrank','savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-painting-deadline-native.log"
