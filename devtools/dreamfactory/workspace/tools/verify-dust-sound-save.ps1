# SPDX-License-Identifier: GPL-3.0-or-later
# Reject incomplete bank tables and invalid priorities without changing the game.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-opening.dfsv")
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$fixtures = "$root/build/save-invalid-dust-sound"
& python "$PSScriptRoot/sound-save-fixtures.py" $RestoreCheckpoint $fixtures
if ($LASTEXITCODE) { throw 'Sound-bank fixture generation failed' }
$cases = @('missing-bank-0','missing-bank-1','unaligned-priority','oversized-priority')
if ([BitConverter]::ToUInt32([IO.File]::ReadAllBytes($RestoreCheckpoint), 4) -ge 5) { $cases += @('unknown-control','oversized-control-count') }
foreach ($name in $cases) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$fixtures/$name.dfsv" -Actions @(
        'expectview north','rejectcheckpoint','expectview north',
        'key rightarrow','expectview east'
    ) *> "$root/build/dust-sound-reject-$name.log"
    Copy-Item "$root/build/boot-dust.log" "$root/build/dust-sound-reject-$name-native.log"
}
Write-Output 'Dust missing/partial sound banks, invalid priorities and supported-format control rejection preserve live navigation PASS'
