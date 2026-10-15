# Regulate the turbine with normal held mouse gestures, then report success.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
$turbineActions = @(
    'dragarc prop valve3 140 297 16 24', 'expectpropdeg valve3 0',
    'dragarc prop valve3 140 297 -16 7', 'expectpropdeg valve3 7', 'expectglobal valve3 36',
    'dragarc prop pump1 372 309 -16 12', 'expectpropdeg pump1 0',
    'dragarc prop pump1 372 309 16 5', 'expectpropdeg pump1 15', 'expectglobal pump1 22',
    'dragpoint prop slider 192 280', 'expectpropdeg slider 7', 'expectglobal coal 68',
    'holduntil prop valve3 electlag 74', 'expectglobalge electlag 74',
    'clickon button exit', 'waitglobal phase 3 60000', 'expectflat main 1',
    'expectset control', 'expectglobal mission 1'
) + $ExtraActions
& "$PSScriptRoot/verify-full-control.ps1" -ClickMovies $ClickMovies -Choices $Choices -ExtraActions $turbineActions -TimeoutSeconds $TimeoutSeconds
if ((Get-Content (Join-Path $PSScriptRoot '../build/boot-full.log') -Raw) -match 'out of mixer slots') {
    throw 'Retail turbine replay exhausted mixer channels'
}
