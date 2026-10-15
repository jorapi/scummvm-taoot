# Meet Max on the boat deck, follow him to the cafe and collect the pipe.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 900)
$ErrorActionPreference = 'Stop'
$maxActions = @(
    'key leftarrow', 'key leftarrow', 'key leftarrow', 'expectview View46',
    'clickon painting door', 'key uparrow', 'expectset deckbd', 'expectview View114',
    'key rightarrow', 'key rightarrow', 'key rightarrow', 'key uparrow',
    'key leftarrow', 'key leftarrow', 'key leftarrow', 'key uparrow',
    'key leftarrow', 'key leftarrow', 'key leftarrow', 'key uparrow',
    'key leftarrow', 'key leftarrow', 'key uparrow', 'expectscene Scene38', 'expectview View128',
    'key rightarrow', 'key rightarrow', 'key rightarrow', 'expectview View133',
    'clickon actor max', 'expectset cafe', 'expectglobal metzeitel 1', 'expectview View44',
    'clickon prop pipe', 'expectflat cuff 1', 'clickon prop cigspipe',
    'expectowner pipe frank', 'clickon button ok', 'expectflat main 1'
) + $ExtraActions
$maxChoices = @('max1.pup:101', 'wilzeit1.pup:103', 'max1.pup:102') + $Choices
& "$PSScriptRoot/verify-full-gym.ps1" -Choices $maxChoices -ClickMovies $ClickMovies -ExtraActions $maxActions -TimeoutSeconds $TimeoutSeconds
