# SPDX-License-Identifier: GPL-3.0-or-later
# Defend against authored attack tells with ordinary mouse movement.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-fencing-ready.dfsv", [int]$TimeoutSeconds = 90, [int]$MaximumParries = 16)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Trace-Parry([string]$Name, [string]$Source, [string[]]$Actions) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $Source -Checkpoint "$root/build/save-full-fencing-parry-$Name.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions ($Actions + @('savecheckpoint'))
    Copy-Item "$root/build/boot-full.log" "$root/build/full-fencing-parry-$Name-native.log"
}
function Assert-ScoreVisible([string]$Name, [int]$Visible) {
    $log = Get-Content "$root/build/full-fencing-parry-$Name-native.log" -Raw
    if ($log -notmatch "BOOT PROP name=williescore .* visible=$Visible ") { throw "Unexpected opponent score after $Name" }
}
Trace-Parry 'initial' $RestoreCheckpoint @('expectflat fence 8','click 256 359','expectglobal fighting 1')
$current = "$root/build/save-full-fencing-parry-initial.dfsv"
$flat = 8
$seen = @{}
for ($turn = 1; $turn -le $MaximumParries -and $seen.Count -lt 2; ++$turn) {
    Trace-Parry "intent$turn" $current @('waitpropviewany willie intentleft|intentright 60000',"expectflat fence $flat",'dumpprop williescore')
    Assert-ScoreVisible "intent$turn" 0
    $intent = "$root/build/save-full-fencing-parry-intent$turn.dfsv"
    $state = & python "$PSScriptRoot/read-checkpoint-globals.py" $intent willieside | ConvertFrom-Json
    if ($LASTEXITCODE -or $state.willieside -notin @('left','right')) { throw 'Invalid authored fencing tell' }
    $side = $state.willieside
    if ($turn -eq 1) { $firstIntent = $intent; $firstSide = $side }
    $x = if ($side -eq 'left') { 100 } else { 400 }
    $flat = [Math]::Max(1, $flat - 1)
    Trace-Parry "blocked$turn" $intent @("expectpropview willie intent$side", "move $x 100",
        "waitglobal playerblock $side 10000",'waitpropview willie charge 60000',
        "expectflat fence $flat",'expectglobal fighting 1','dumpprop williescore')
    Assert-ScoreVisible "blocked$turn" 0
    $seen[$side] = $true
    $current = "$root/build/save-full-fencing-parry-blocked$turn.dfsv"
}
if ($seen.Count -ne 2) { throw "Did not parry both sides within $MaximumParries attacks" }
Trace-Parry 'restored' $current @("expectflat fence $flat",'expectglobal fighting 1','dumpprop williescore')
Assert-ScoreVisible 'restored' 0
$wrong = if ($firstSide -eq 'left') { 'right' } else { 'left' }
$x = if ($wrong -eq 'left') { 100 } else { 400 }
Trace-Parry 'wrong-side' $firstIntent @("expectpropview willie intent$firstSide", "move $x 100",
    "waitglobal playerblock $wrong 10000",'waitglobal fighting 0 60000','expectflat fence 8','dumpprop williescore')
Assert-ScoreVisible 'wrong-side' 1
Write-Output 'Titanic left/right parries, active-attack restores and wrong-side hit PASS'
