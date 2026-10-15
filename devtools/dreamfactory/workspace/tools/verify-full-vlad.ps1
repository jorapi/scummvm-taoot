# Meet Vlad safely after hiding the Rubaiyat, and accept his package request.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
$vladActions = @(
    'key leftarrow', 'key uparrow', 'key leftarrow', 'key leftarrow',
    'key uparrow', 'key rightarrow', 'expectscene Scene40', 'expectview View45',
    'waitactorwalk vlad 0', 'clickon actor vlad', 'expectactorowner vlad help',
    'expectflat main 1', 'expectglobal phase 3', 'expectglobal playerdeath ',
    'expectowner rubaiyat coal5'
) + $ExtraActions
$vladChoices = @('vlad1.pup:101', 'vlad1.pup:102', 'vlad1.pup:102', 'vlad1.pup:101') + $Choices
& "$PSScriptRoot/verify-full-rubaiyat-stash.ps1" -ClickMovies $ClickMovies -Choices $vladChoices -ExtraActions $vladActions -TimeoutSeconds $TimeoutSeconds
