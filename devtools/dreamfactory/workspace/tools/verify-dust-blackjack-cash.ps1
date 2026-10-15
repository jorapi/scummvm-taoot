# SPDX-License-Identifier: GPL-3.0-or-later
# Play ordinary blackjack bets using visible totals: Hit below 17, otherwise Stand.
param(
    [string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-blackjack-cash.dfsv",
    [int]$MinimumCash = 6, [int]$MaximumHands = 12, [int]$TimeoutSeconds = 180,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$ResultPrefix = 'cash',
    [switch]$SkipSpeech = ($env:DREAMFACTORY_BOOT_SKIP_SPEECH -eq '1'),
    [ValidateRange(1,10)][int]$BetSize = 1,
    [ValidateRange(1,10)][int[]]$BetSequence = @(),
    [switch]$PlayAllHands,
    [ValidateRange(12,20)][int]$HitBelow = 17
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Read-Blackjack([string]$Checkpoint) {
    $json = & python "$PSScriptRoot/read-checkpoint-globals.py" $Checkpoint playercash playerbet playertotal playercount winner playerphase
    if ($LASTEXITCODE) { throw 'Cannot inspect the saved visible hand state' }
    $json | ConvertFrom-Json
}
function Invoke-CashTrace([string]$Name,[string]$Restore,[string[]]$Actions,[string[]]$Choices=@()) {
    $checkpoint = "$root/build/save-dust-$Name.dfsv"
    try {
        & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $Restore -Checkpoint $checkpoint -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices $Choices -Actions $Actions -SkipSpeech:$SkipSpeech *> "$root/build/dust-$Name-run.log"
    } finally {
        Copy-Item "$root/build/boot-dust.log" "$root/build/dust-$Name-native.log"
    }
    $checkpoint
}
$restore = $RestoreCheckpoint
$state = Read-Blackjack $restore
for ($hand=1; $hand -le $MaximumHands -and $state.playercash -gt 0 -and ($PlayAllHands -or $state.playercash -lt $MinimumCash); $hand++) {
    $name = "$ResultPrefix-hand$hand"
    $before = $state.playercash
    $requestedBet = if ($BetSequence.Count) { $BetSequence[[Math]::Min($hand - 1, $BetSequence.Count - 1)] } else { $BetSize }
    $amount = [Math]::Min($requestedBet, $before)
    $betChoices = @('jan.pup:101') + @(for ($chip = 0; $chip -lt $amount; $chip++) { 'jan.pup:101' }) + @('jan.pup:111')
    $restore = Invoke-CashTrace "$name-ready" $restore @(
        'expectset sallower','expectscene Scene C4','expectview west','expectflat mainpanel',
        'clickon prop blackjack','expectflat Flat 2',"expectglobal playerbet $amount",'savecheckpoint'
    ) $betChoices
    $state = Read-Blackjack $restore
    for ($hit=1; !$state.winner -and $state.playertotal -lt $HitBelow; $hit++) {
        if ($hit -gt 12) { throw 'Too many draws in a blackjack hand' }
        $count = $state.playercount
        $restore = Invoke-CashTrace "$name-hit$hit" $restore @('clickon button hit','savecheckpoint')
        $state = Read-Blackjack $restore
        if ($state.playercount -le $count) { throw 'Hit input did not draw a card' }
    }
    if (!$state.winner) {
        $restore = Invoke-CashTrace "$name-standing" $restore @('clickon button stay','savecheckpoint')
        $state = Read-Blackjack $restore
    }
    $actions = @('expectflat Flat 2')
    $actions += @('waitglobal playcards 0 60000','expectflat mainpanel','savecheckpoint')
    $choices = if ($state.playercash -gt 0) { @('jan.pup:112') } else { @() }
    $restore = Invoke-CashTrace "$name-result" $restore $actions $choices
    $state = Read-Blackjack $restore
    Write-Output "Dust blackjack hand $hand cash $before -> $($state.playercash)"
}
$resultName = if ($ResultPrefix -eq 'cash') { 'casino' } else { $ResultPrefix }
Copy-Item $restore "$root/build/save-dust-$resultName-result.dfsv"
if ($state.playercash -lt $MinimumCash) { throw "Cash target not reached through these hands: $($state.playercash)" }
Write-Output "Dust blackjack cash requirement PASS: $($state.playercash)"
