# SPDX-License-Identifier: GPL-3.0-or-later
# Read earned snapshots and issue ordinary attacks; never modify game state.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-fencing-ready.dfsv", [int]$MaximumAttacks = 80, [int]$TimeoutSeconds = 180, [ValidatePattern('^[a-z0-9-]+$')][string]$TracePrefix = 'fencing-win')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$current = $RestoreCheckpoint
$initial = & python "$PSScriptRoot/read-checkpoint-globals.py" $current fencewins fencecount | ConvertFrom-Json
if ($LASTEXITCODE) { throw 'Cannot read the earned fencing checkpoint' }
$wanted = [int]$initial.fencewins + 1
$wantedCount = [int]$initial.fencecount + 1
$points = 0
for ($attack = 1; $attack -le $MaximumAttacks; ++$attack) {
    $state = & python "$PSScriptRoot/read-checkpoint-globals.py" $current fighting willieblock attacktot fencewins | ConvertFrom-Json
    if ($LASTEXITCODE) { throw 'Cannot inspect the current fencing snapshot' }
    if ([int]$state.fencewins -eq $wanted) { break }
    $a = @('expectset squash')
    if (!$state.fighting) { $a += 'click 256 359' }
    # The authored third-or-later attack scores when aimed at an opening.
    # Select only a physical screen quadrant from the game's current block list.
    $quad = 'UR'
    $opening = @()
    if ([int]$state.attacktot -ge 2) {
        $opening = @($state.willieblock -split ';' | Where-Object { $_ -in @('UR','LR','UL','LL') })
        if ($opening.Count) { $quad = $opening[0] }
    }
    # Only the fifth scoring attack enters the post-bout conversation.
    # If no opening is available there, wait for the opponent's next stance.
    $final = $points -eq 4 -and [int]$state.attacktot -ge 2 -and $opening.Count -gt 0
    $x = if ($quad.EndsWith('R')) { 400 } else { 100 }
    $y = if ($quad.StartsWith('U')) { 100 } else { 300 }
    if ($points -eq 4 -and [int]$state.attacktot -ge 2 -and !$opening.Count) {
        $a += @('move 100 100', 'idle 10')
    } else { $a += "click $x $y" }
    if (!$final) { $a += @('dumpprop playerscore', 'dumpprop williescore') }
    $a += 'savecheckpoint'
    $next = "$root/build/save-full-$TracePrefix-attack$attack.dfsv"
    $choices = @()
    if ($final) {
        # The second bout adds two mandatory conversation branches before
        # the ordinary invitation to fence again.
        if ($wantedCount -eq 2) { $choices += @('wilfenc1.pup:101','wilfenc1.pup:101','wilfenc1.pup:101','wilfenc1.pup:102','wilfenc1.pup:101') }
        $choices += 'wilfenc1.pup:102'
    }
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $current -Checkpoint $next -Steps 0 -TimeoutSeconds $TimeoutSeconds -Choices $choices -Actions $a
    Copy-Item "$root/build/boot-full.log" "$root/build/full-$TracePrefix-attack$attack-native.log"
    $log = Get-Content "$root/build/boot-full.log" -Raw
    if ($log -match 'BOOT PROP name=playerscore deg=(\d+).* visible=1') { $points = [int]$Matches[1] + 1 }
    $current = $next
}
$state = & python "$PSScriptRoot/read-checkpoint-globals.py" $current fencewins | ConvertFrom-Json
if ($LASTEXITCODE -or [int]$state.fencewins -ne $wanted) { throw "No winning bout within $MaximumAttacks physical attacks" }
$wonName = if ($TracePrefix -eq 'fencing-win') { 'fencing-won' } else { "$TracePrefix-won" }
Copy-Item $current "$root/build/save-full-$wonName.dfsv"
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-$wonName.dfsv" -Steps 1 -Actions @(
    "expectglobal fencewins $wanted", 'expectactorowner willie won', "expectglobal fencecount $wantedCount",
    'expectset sqhall', 'expectflat main 1', 'savecheckpoint')
Copy-Item "$root/build/boot-full.log" "$root/build/full-$wonName-restore-native.log"
Write-Output "Retail winning fencing bout PASS after $($attack - 1) physical attacks"
