# Wait out the scripted raid, then reach Frank's cabin aboard Titanic.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$boardingActions = @(
    'waitset c73 60000', 'expectscene Scene51', 'expectview View63',
    'expectglobal mission 1', 'expectglobal phase 0',
    'expectglobal hrs 9', 'expectglobal min 30'
) + $ExtraActions
& "$PSScriptRoot/verify-full-bedsit.ps1" -Choices $Choices -ClickMovies $ClickMovies -SkipMovies @('bedex.mov', 'ocredits.mov', 'datecab.mov') -ExtraActions $boardingActions -TimeoutSeconds $TimeoutSeconds
