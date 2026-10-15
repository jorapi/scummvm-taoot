# Reach the dog gate and ask the help character for the bone.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$boneChoices = @('help1.pup:103', 'help1.pup:102', 'help1.pup:101') + $Choices
$boneActions = @(
    'key uparrow', 'key uparrow', 'key uparrow', 'expectscene Scene G12',
    'key uparrow', 'expectscene Scene G12', 'key rightarrow', 'expectview east',
    'clickon actor help', 'expectpropview bone small'
) + $ExtraActions
& "$PSScriptRoot/verify-dust-opening.ps1" -Choices $boneChoices -ExtraActions $boneActions -TimeoutSeconds $TimeoutSeconds
