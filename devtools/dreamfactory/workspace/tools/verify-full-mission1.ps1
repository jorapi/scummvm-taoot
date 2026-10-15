# Complete the Rubaiyat mission by reporting the book and clues to Penny.
param([string[]]$BeforeDeliveryActions = @(), [string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
function Get-ReportRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
$reportActions = @(Get-ReportRoute BOIL View15 View47) + @(
    'clickon painting door', 'key uparrow', 'expectglobal savedeck boil2', 'expectview View46'
) + @(Get-ReportRoute BOIL View46 View44) + @(
    'clickon painting door', 'key uparrow', 'expectglobal savedeck boil1', 'expectview View45'
) + @(Get-ReportRoute BOIL View45 View47) + @(
    'clickon painting door', 'key uparrow', 'expectset engine', 'expectview View71',
    'clickon prop map', 'key f', 'expectflat Map 7', 'clickon button Button22',
    'expectset stair2c', 'expectview View28', 'clickon painting door',
    'key uparrow', 'expectset hallf2c', 'expectview View70'
) + @(Get-ReportRoute HALLF2C View70 View76) + @(
    'expectscene Scene59', 'expectview View76', 'clickon painting knock',
    'expectglobal mission 2', 'expectglobal phase 0', 'expectflat main 1',
    'expectowner rubaiyat frank', 'expectowner vladfood vlad', 'expectglobal playerdeath '
) + $ExtraActions
$reportChoices = @('penny1.pup:101', 'penny1.pup:104', 'penny1.pup:102', 'penny1.pup:105', 'penny1.pup:109') + $Choices
& "$PSScriptRoot/verify-full-package.ps1" -BeforeDeliveryActions $BeforeDeliveryActions -ClickMovies (@('sasha.mov') + $ClickMovies) -Choices $reportChoices -ExtraActions $reportActions -TimeoutSeconds $TimeoutSeconds
