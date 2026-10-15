# Pick up the bone, offer it to the dog, and receive the help character's ring.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$ringChoices = @('help1.pup:102') + $Choices
$ringActions = @(
    'key rightarrow', 'expectview south', 'key uparrow', 'key uparrow', 'key uparrow',
    'expectscene Scene G15', 'key rightarrow', 'expectview west',
    'clickon prop bone', 'expectowner bone stranger', 'expectglobal handitem bone',
    'key rightarrow', 'expectview north', 'key uparrow', 'key uparrow', 'key uparrow',
    'expectscene Scene G12', 'dragto prop bone actor dog',
    'expectglobal phase 2', 'expectglobal helpphase 3', 'expectowner ring stranger'
) + $ExtraActions
& "$PSScriptRoot/verify-dust-bone.ps1" -Choices $ringChoices -ExtraActions $ringActions -TimeoutSeconds $TimeoutSeconds
