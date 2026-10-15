# Reach Penny upstairs and accept the supplied demo's photograph assignment.
# Uses authored navigation and dialogue through ordinary native input events.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$missionChoices = @('dpenny.pup:101', 'dpenny.pup:101') + $Choices
$missionActions = @(
    'key uparrow', 'key rightarrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow', 'key rightarrow',
    'key uparrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow',
    'expectset gstair1', 'expectscene Scene15', 'expectview View27',
    'key leftarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'expectscene Scene18', 'expectview View38',
    'clickon actor penny', 'expectglobal phase 1'
) + $ExtraActions
& "$PSScriptRoot/verify-demo-story.ps1" -Choices $missionChoices -ExtraActions $missionActions -TimeoutSeconds $TimeoutSeconds
