# Pick up the jug, meet Jones outside the saloon and receive his cards.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$cardsChoices = @('jones.pup:104') + $Choices
$cardsActions = @(
    'key rightarrow', 'expectview south', 'key uparrow', 'key uparrow', 'key uparrow',
    'expectscene Scene G15', 'key rightarrow', 'key rightarrow', 'expectview north',
    # The jug refuses pickup while Leroy still guards it (y > 3200). Fast
    # dialogue skips can reach it before his authored walk to the range ends.
    'waitactorstar leroy town.leroy2 90000',
    'clickon prop jug', 'expectowner jug stranger', 'waitactorstar jones town.jones2',
    'key uparrow', 'key uparrow', 'key uparrow', 'key uparrow', 'expectscene Scene G11',
    'clickon actor jones', 'expectglobal jonesphase 1', 'expectowner cards stranger',
    'expectglobal handitem cards'
) + $ExtraActions
& "$PSScriptRoot/verify-dust-ring.ps1" -Choices $cardsChoices -ExtraActions $cardsActions -TimeoutSeconds $TimeoutSeconds
