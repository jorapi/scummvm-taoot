# Move the recovered Rubaiyat from chute four to chute five using inventory.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
$stashActions = @(
    'key rightarrow', 'key uparrow', 'key rightarrow', 'expectview View15',
    'clickon painting coal5', 'expectflat boil 1', 'clickon prop boildoor',
    'clickon prop boilbag', 'expectflat inven 1', 'clickon prop rubaiyat',
    'expectglobal handitem rubaiyat', 'clickthen button ok 150 250 3000',
    'expectflat boil 1', 'expectowner rubaiyat coal5', 'expectglobal handitem ',
    'expectpropview boildoor idleopen', 'clickon prop boildoor',
    'expectpropview boildoor idleclosed', 'clickon button ok',
    'expectflat main 1', 'expectglobal phase 3', 'expectglobal coalchute coal4'
) + $ExtraActions
& "$PSScriptRoot/verify-full-rubaiyat.ps1" -ClickMovies $ClickMovies -Choices $Choices -ExtraActions $stashActions -TimeoutSeconds $TimeoutSeconds
