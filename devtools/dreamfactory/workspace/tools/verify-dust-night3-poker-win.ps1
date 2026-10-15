# SPDX-License-Identifier: GPL-3.0-or-later
# Natural first-hand win, authored Thunderbird award and fresh restoration.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-night3-poker-natural-ready.dfsv", [switch]$PrepareStake, [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if ($PrepareStake) {
    & "$PSScriptRoot/verify-dust-night3-poker-stake.ps1" -TimeoutSeconds $TimeoutSeconds
    & "$PSScriptRoot/find-dust-night3-poker-hand.ps1" -RestoreCheckpoint "$root/build/save-dust-night3-poker-funded.dfsv" -TimeoutSeconds $TimeoutSeconds
    $RestoreCheckpoint = "$root/build/save-dust-night3-poker-natural-ready.dfsv"
}
$json = & python "$PSScriptRoot/dust-poker-plan.py" $RestoreCheckpoint
if ($LASTEXITCODE) { throw 'Cannot plan the earned hand' }
$plan = $json | ConvertFrom-Json
if (!$plan.winsWithoutFolds) { throw 'The checkpoint does not have a natural winning draw' }
function Trace-Poker([string]$Name, [string]$Source, [string[]]$Actions, [string[]]$Choices = @()) {
    $checkpoint = "$root/build/save-dust-night3-poker-$Name.dfsv"
    try {
        & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $Source -Checkpoint $checkpoint -TimeoutSeconds $TimeoutSeconds -Choices $Choices -Actions ($Actions + @('savecheckpoint'))
    } finally { Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night3-poker-$Name-native.log" }
    return $checkpoint
}
function Read-Poker([string]$Source) {
    $json = & python "$PSScriptRoot/read-checkpoint-globals.py" $Source playerphase winner
    if ($LASTEXITCODE) { throw 'Cannot inspect poker phase' }
    return ($json | ConvertFrom-Json)
}
# Suppress routine trace output in the returned checkpoint pipeline.
$source = "$root/build/save-dust-night3-poker-natural-bet1.dfsv"
$null = Trace-Poker 'natural-bet1' $RestoreCheckpoint @('expectglobal phase 1','expectowner tbird none','clickon prop continue') @('mez.pup:101')
$state = Read-Poker $source
if ($state.playerphase -eq 2) {
    $actions = @($plan.discards | ForEach-Object { "clickon prop $_" }) + @("expectglobal playercount $(5 - $plan.discards.Count)",'clickon prop continue')
    if ($plan.discards.Count) {
        $null = Trace-Poker 'natural-drawn' $source ($actions + @('expectglobal playerphase 3'))
        $source = "$root/build/save-dust-night3-poker-natural-drawn.dfsv"
        $actions = @('clickon prop continue')
    }
    $null = Trace-Poker 'natural-won' $source ($actions + @('expectglobal playerphase 4','expectglobal winner player',"expectglobal playertotal $($plan.score)",'expectglobal phase 1')) @('mez.pup:101')
    $source = "$root/build/save-dust-night3-poker-natural-won.dfsv"
} elseif ($state.playerphase -ne 4 -or $state.winner -ne 'player') { throw 'Unexpected first betting result' }
$null = Trace-Poker 'thunderbird' $source @(
    'expectglobal phase 1','expectglobal winner player','clickon prop continue',
    'expectflat mainpanel','expectglobal phase 2','expectglobal mezphase 2',
    'expectowner tbird stranger','expectowner cards stranger','expectglobal hasquitpoker 1',
    'expectglobal playerdeath ','expectglobalge playercash 23',
    'key rightarrow','expectview south','key leftarrow','expectview east'
) @('mez.pup:102')
$null = Trace-Poker 'thunderbird-restored' "$root/build/save-dust-night3-poker-thunderbird.dfsv" @(
    'expectowner tbird stranger','expectowner cards stranger','expectglobal phase 2',
    'expectglobal mezphase 2','expectflat mainpanel',
    'key rightarrow','expectview south','key leftarrow','expectview east'
)
Write-Output 'Dust third-night natural poker win, Thunderbird award, phase progression and fresh restored navigation PASS'
