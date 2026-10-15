# Enter blackjack and return to the saloon, optionally playing one $1 hand.
param([switch]$PlayHand, [string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 420)
$ErrorActionPreference = 'Stop'
$blackjackChoices = if ($PlayHand) { @('jan.pup:101', 'jan.pup:101', 'jan.pup:111', 'jan.pup:112') } else { @('jan.pup:102') }
$blackjackChoices += $Choices
$blackjackActions = @(
    'key rightarrow', 'key rightarrow', 'expectview east',
    'key uparrow', 'expectscene Scene C4',
    'key rightarrow', 'key rightarrow', 'expectview west',
    'clickon prop blackjack', 'expectset sallower'
)
if ($PlayHand) {
    $blackjackActions += @(
        'expectflat Flat 2', 'expectglobal playerbet 1', 'expectglobal playercount 2',
        'expectglobal dealercount 2', 'expectglobal playerphase 1',
        # A natural blackjack has already settled the hand and armed Jan's
        # next-bet prompt, even though newgame writes playerphase=1 afterward.
        'clickifempty winner button stay', 'waitglobal playcards 0 30000', 'expectflat mainpanel'
    )
} else { $blackjackActions += @('expectflat mainpanel', 'expectglobal playercash 5') }
$blackjackActions += $ExtraActions
& "$PSScriptRoot/verify-dust-saloon.ps1" -Choices $blackjackChoices -ExtraActions $blackjackActions -TimeoutSeconds $TimeoutSeconds
