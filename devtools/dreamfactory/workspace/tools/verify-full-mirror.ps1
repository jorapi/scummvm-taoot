# Read the Turkish-bath mirror clue after collecting Max's pipe.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1200)
$ErrorActionPreference = 'Stop'
$mirrorActions = @(
    'clickon prop map', 'expectflat Map 3',
    'key f', 'expectflat Map 7', 'clickon button Button44',
    'expectset turkstrs', 'expectscene Scene10', 'expectview View16',
    'clickon painting door', 'key uparrow', 'expectset turk', 'expectview View116',
    'key uparrow', 'key rightarrow', 'key rightarrow', 'key uparrow',
    'key leftarrow', 'key leftarrow', 'expectview View119',
    'clickon painting water', 'expectflat cuff 1', 'clickon button knob',
    'clickon button ok', 'expectflat main 1', 'expectglobal turkwater 1',
    'key rightarrow', 'key uparrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'expectview View133',
    'clickon painting mirror', 'waitglobal phase 2 60000',
    'expectglobal zeitclue mirror', 'expectglobal turkwater 0'
) + $ExtraActions
& "$PSScriptRoot/verify-full-max.ps1" -Choices $Choices -ClickMovies (@('turkmes.mov') + $ClickMovies) -ExtraActions $mirrorActions -TimeoutSeconds $TimeoutSeconds
