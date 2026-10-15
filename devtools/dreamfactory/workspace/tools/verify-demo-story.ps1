# Follow the supplied September 1996 demo's opening story to the Grand Staircase.
# All transitions and state changes come from the original game scripts.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$openingChoices = @(
    'dsmeth.pup:101', 'dsmeth.pup:102', 'dsmeth.pup:103'
) + $Choices
$openingActions = @(
    'clickon painting halla', 'expectglobal smethphase 400',
    'clickon painting halla', 'expectowner door c71',
    'key uparrow', 'expectset halla', 'expectscene Scene52', 'expectview View63',
    'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow',
    'key leftarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow',
    'expectscene Scene72', 'expectview View96',
    'clickon painting gstair2,decka', 'expectowner door halla-gs2port',
    'key uparrow', 'expectset gstair2', 'expectscene Scene11', 'expectview View23'
) + $ExtraActions
# Both movies have authored click-to-continue regions; neither is auto-skipped.
& "$PSScriptRoot/verify-gameplay.ps1" -Editions demo -ClickMovies @('penote.mov', 'postdemo.mov') -Choices $openingChoices -ExtraActions $openingActions -TimeoutSeconds $TimeoutSeconds
