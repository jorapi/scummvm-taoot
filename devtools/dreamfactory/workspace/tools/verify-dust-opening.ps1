# Talk to Leroy at the sign and leave through the authored dialogue branch.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$openingChoices = @('leroy.pup:104') + $Choices
$openingActions = @(
    'clickon button OK', 'expectflat mainpanel', 'key leftarrow', 'expectview north',
    'clickon actor leroy', 'expectglobal leroyphase 1'
) + $ExtraActions
& "$PSScriptRoot/verify-gameplay.ps1" -Editions dust -Choices $openingChoices -ExtraActions $openingActions -TimeoutSeconds $TimeoutSeconds
