# SPDX-License-Identifier: GPL-3.0-or-later
# Continuous-input routes can save between clock normalization passes.
# Verify ordinary idle processing resumes the authored sinking clock.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-sinking-notebook.dfsv", [int]$TimeoutSeconds = 60)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$before = & python "$PSScriptRoot/read-checkpoint-globals.py" $RestoreCheckpoint hrs min sec | ConvertFrom-Json
if ($LASTEXITCODE) { throw 'Cannot inspect the earned sinking clock' }
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-sinking-clock.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal mission 4', 'expectglobal phase 0', 'expectowner notebook frank',
    'expectowner shawl vlad', 'idle 25', 'expectglobalge clock 1300',
    'expectglobal phase 0', 'savecheckpoint'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-sinking-clock-native.log"
$after = & python "$PSScriptRoot/read-checkpoint-globals.py" "$root/build/save-full-sinking-clock.dfsv" hrs min sec clock | ConvertFrom-Json
if ($LASTEXITCODE) { throw 'Cannot inspect the resumed sinking clock' }
# advanceday randomizes seconds; calctime normalizes the string clock without
# resetting that sub-minute value. Check actual progression across a rollover.
$beforeSeconds = ([int]$before.hrs * 60 + [int]$before.min) * 60 + [int]$before.sec
$afterSeconds = ([int]$after.hrs * 60 + [int]$after.min) * 60 + [int]$after.sec
if ($after.sec -lt 0 -or $after.sec -ge 60 -or $after.clock -ne ([int]$after.hrs * 100 + [int]$after.min) -or $afterSeconds -le $beforeSeconds) { throw 'Sinking clock did not normalize and advance through ordinary idle processing' }
Write-Output "Retail sinking clock normalization and progression PASS: $beforeSeconds -> $afterSeconds"
