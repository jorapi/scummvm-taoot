# Collect Frank's kit, take Smethells's map and leave C73 for the C-deck hall.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$stewardChoices = @('smeth1.pup:101', 'smeth1.pup:102', 'smeth1.pup:104') + $Choices
$stewardActions = @(
    'clickon prop bag', 'expectowner bag frank',
    'key leftarrow', 'key leftarrow', 'key leftarrow',
    'clickon prop watch', 'expectowner watch frank',
    'key rightarrow', 'key rightarrow', 'key rightarrow', 'expectview View63',
    'key uparrow', 'expectscene Scene49',
    'expectview View55',
    'clickon painting door', 'expectglobal smethphase 1', 'expectowner map frank',
    'clickon painting door',
    'key uparrow', 'expectset hallc', 'expectscene Scene27', 'expectview View35'
) + $ExtraActions
& "$PSScriptRoot/verify-full-boarding.ps1" -Choices $stewardChoices -ClickMovies (@('penote.mov') + $ClickMovies) -ExtraActions $stewardActions -TimeoutSeconds $TimeoutSeconds
