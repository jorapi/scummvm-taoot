# Score the seven London-flat objects through ordinary close-up movie clicks.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$SkipMovies = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$bedsitActions = @(
    'clickon painting memory', 'expectglobal bombpoints 4',
    'clickon painting mantle', 'expectglobal bombpoints 5',
    'key leftarrow', 'key leftarrow', 'expectview View13',
    'clickon painting obit', 'expectglobal bombpoints 7',
    'key leftarrow', 'expectview View17',
    'clickon painting paper', 'expectglobal bombpoints 8',
    'clickon painting cards', 'expectglobal bombpoints 9',
    'key leftarrow', 'expectview View15',
    'clickon painting cabinet', 'expectglobal bombpoints 10',
    'key leftarrow', 'expectview View19',
    'clickon painting poster', 'expectglobal bombpoints -20000'
) + $ExtraActions
$closeups = @('bedmem.mov', 'bedmant.mov', 'bedobit.mov', 'bedpaper.mov',
    'bedcards.mov', 'bedcab.mov', 'bedpost.mov') + $ClickMovies
& "$PSScriptRoot/verify-gameplay.ps1" -Editions full -Choices $Choices -ClickMovies $closeups -SkipMovies $SkipMovies -ExtraActions $bedsitActions -TimeoutSeconds $TimeoutSeconds
