# Reach Penny upstairs and accept the supplied demo's photograph assignment.
# Uses authored navigation and dialogue through ordinary native input events.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
function Plan($room, $from, $to) {
    $route = @(& node "$PSScriptRoot/room-route.mjs" demo $room $from $to | ForEach-Object { ($_ -split ' # ')[0] })
    if ($LASTEXITCODE) { throw "Cannot plan the demo $room route" }
    return $route
}
$missionChoices = @('dpenny.pup:101', 'dpenny.pup:101') + $Choices
$missionActions = @(Plan GSTAIR2 View23 View73) + @(
    'expectset gstair1', 'expectview View27'
) + @(Plan GSTAIR1 View27 View38) + @(
    'expectscene Scene18', 'expectview View38',
    'clickon actor penny', 'expectglobal phase 1'
) + $ExtraActions
& "$PSScriptRoot/verify-demo-story.ps1" -Choices $missionChoices -ExtraActions $missionActions -TimeoutSeconds $TimeoutSeconds
