# SPDX-License-Identifier: GPL-3.0-or-later
# Play an actual match using physical drags planned from read-only board snapshots.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-checkers-ready.dfsv", [int]$Depth = 6, [int]$MaximumTurns = 100, [ValidateSet('wineasy','winmedium','winhard','lose')][string]$ExpectedScore = 'wineasy', [int]$TimeoutSeconds = 240, [string]$TracePrefix = 'dust-checkers', [ValidatePattern('^[a-z0-9-]+$')][string]$ResultPrefix = 'dust-checkers')
$ErrorActionPreference = 'Stop'
if ($TracePrefix -notmatch '^[a-z0-9-]+$') { throw 'Invalid trace filename prefix' }
$root = Split-Path $PSScriptRoot -Parent
if (!(Test-Path "$root/build/reference-checkers/checkers.mjs")) {
    & node "$PSScriptRoot/checkers-fixtures.mjs"
    if ($LASTEXITCODE) { throw 'Cannot prepare the independent move planner' }
}
$current = $RestoreCheckpoint
for ($turn = 1; $turn -le $MaximumTurns; ++$turn) {
    $before = & python "$PSScriptRoot/read-checkpoint-globals.py" $current mainboard gameover score | ConvertFrom-Json
    if ($LASTEXITCODE) { throw 'Cannot read the earned board' }
    if ($before.gameover) { break }
    $strategy = if ($ExpectedScore -eq 'lose') { 'lose' } else { 'best' }
    $moves = @(& node "$PSScriptRoot/dust-checkers-move.mjs" $before.mainboard $Depth $strategy)
    if ($LASTEXITCODE -or !$moves.Count) { throw 'Cannot plan a legal player turn' }
    $next = "$root/build/save-$TracePrefix-turn$turn.dfsv"
    & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $current -Checkpoint $next -TimeoutSeconds $TimeoutSeconds -Actions (@('expectflat flat 0') + $moves + @('savecheckpoint'))
    Copy-Item "$root/build/boot-dust.log" "$root/build/$TracePrefix-turn$turn-native.log"
    $after = & python "$PSScriptRoot/read-checkpoint-globals.py" $next mainboard doublejumper playerturn gameover score | ConvertFrom-Json
    if ($LASTEXITCODE -or $before.mainboard -eq $after.mainboard) { throw 'The physical turn did not change the board' }
    if (!$after.gameover -and ($after.doublejumper -or !$after.playerturn)) { throw 'The player turn did not finish its capture chain and opponent reply' }
    $current = $next
    Write-Output "Native checkers turn $turn PASS gameover=$($after.gameover) score=$($after.score)"
}
$state = & python "$PSScriptRoot/read-checkpoint-globals.py" $current gameover score | ConvertFrom-Json
if ($LASTEXITCODE -or !$state.gameover -or $state.score -ne $ExpectedScore) { throw "Expected $ExpectedScore within $MaximumTurns turns; got $($state.score)" }
$name = if ($ExpectedScore -eq 'lose') { 'lost' } else { 'won' }
$terminal = "$root/build/save-$ResultPrefix-$name.dfsv"
if ((Resolve-Path $current).Path -ne [IO.Path]::GetFullPath($terminal)) { Copy-Item $current $terminal }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $current -Checkpoint "$root/build/save-$ResultPrefix-$name-return.dfsv" -TimeoutSeconds $TimeoutSeconds -Choices @('bolivar.pup:104') -Actions @(
    "expectglobal score $ExpectedScore", 'waitglobal bolivarphase 2 60000', 'expectglobal score 0', 'expectset store', 'expectflat mainpanel',
    'expectscene Scene C2', 'expectview north','key rightarrow','expectview east','key leftarrow','expectview north','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/$ResultPrefix-$name-return-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-$ResultPrefix-$name-return.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal score 0','expectset store','expectflat mainpanel','expectscene Scene C2','expectview north',
    'key rightarrow','expectview east','key leftarrow','expectview north')
Copy-Item "$root/build/boot-dust.log" "$root/build/$ResultPrefix-$name-return-restore-native.log"
Write-Output "Dust $ExpectedScore match, postgame conversation and restored store navigation PASS"
