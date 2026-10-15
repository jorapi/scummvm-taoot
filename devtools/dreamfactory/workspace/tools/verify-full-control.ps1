# Walk from the mirror clue to the engineer and enter the turbine puzzle.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
function Get-RoomActions([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
$controlActions = @(
    'clickon prop map', 'expectflat Map 7', 'clickon button Button22',
    'expectset stair2c', 'expectview View28',
    'clickon painting door', 'key uparrow', 'expectset hallf2c', 'expectview View70'
) + @(Get-RoomActions HALLF2C View70 View13) + @(
    'expectview View13', 'clickon painting door', 'key uparrow',
    'expectset turb', 'expectview View89'
) + @(Get-RoomActions TURB View89 View74) + @(
    'expectview View74', 'clickon painting door', 'key uparrow',
    'expectset control', 'expectview View13'
) + @(Get-RoomActions CONTROL View13 View24) + @(
    'expectview View24', 'clickon painting controls', 'expectflat turbine 1',
    'expectglobal mission 1', 'expectglobal phase 2'
) + $ExtraActions
$controlChoices = @('csea1.pup:102', 'csea1.pup:103') + $Choices
& "$PSScriptRoot/verify-full-mirror.ps1" -ClickMovies $ClickMovies -Choices $controlChoices -ExtraActions $controlActions -TimeoutSeconds $TimeoutSeconds
