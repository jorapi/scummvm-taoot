# Walk from the boat-deck map destination to Penny's first mission briefing.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 480)
$ErrorActionPreference = 'Stop'
$gymActions = @(
    'key leftarrow', 'expectview View31', 'key uparrow', 'expectscene Scene19',
    'key rightarrow', 'key rightarrow', 'expectview View43',
    'clickon painting door', 'key uparrow', 'expectset veststbd', 'expectview View18',
    'key rightarrow', 'expectview View21', 'clickon painting door',
    'key uparrow', 'expectset gym', 'expectview View47',
    # Penny ignores clicks while her authored idle turn is in progress.
    'waitactorwalk penny 0', 'clickon actor penny',
    'expectglobal mission 1', 'waitglobal phase 1 60000'
) + $ExtraActions
$gymChoices = @('penny1.pup:103', 'penny1.pup:103', 'penny1.pup:102', 'penny1.pup:101') + $Choices
& "$PSScriptRoot/verify-full-map.ps1" -Choices $gymChoices -ClickMovies (@('zeit.mov') + $ClickMovies) -ExtraActions $gymActions -TimeoutSeconds $TimeoutSeconds
