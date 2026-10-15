# Use the acquired deck map to travel to the boat-deck Grand Staircase.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$mapActions = @(
    'clickon prop map', 'expectpropview map light', 'clickon prop map',
    'expectflat Map 4', 'clickon button Button18', 'expectflat Map 1',
    'clickon button Button27', 'expectset gstair1',
    'expectscene Scene17', 'expectview View47'
) + $ExtraActions
& "$PSScriptRoot/verify-full-steward.ps1" -Choices $Choices -ClickMovies $ClickMovies -ExtraActions $mapActions -TimeoutSeconds $TimeoutSeconds
