# Receive Vlad's package through Sasha's authored cabin-door conversation.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
function Get-SashaRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
$sashaActions = @(Get-SashaRoute BOIL View45 View47) + @(
    'clickon painting door', 'key uparrow', 'expectglobal savedeck boil2', 'expectview View46'
) + @(Get-SashaRoute BOIL View46 View44) + @(
    'clickon painting door', 'key uparrow', 'expectglobal savedeck boil1', 'expectview View45'
) + @(Get-SashaRoute BOIL View45 View47) + @(
    'clickon painting door', 'key uparrow', 'expectset engine', 'expectview View71',
    'clickon prop map', 'key a', 'expectflat Map 2', 'clickon button Button34',
    'expectset gstair2', 'expectscene Scene14', 'expectview View37'
) + @(Get-SashaRoute GSTAIR2 View37 View22) + @(
    'expectview View22', 'clickon painting door', 'key uparrow',
    'expectset halla', 'expectview View95', 'expectglobal hallside port'
) + @(Get-SashaRoute HALLA View95 View57) + @(
    'expectscene Scene51', 'expectview View57', 'clickon painting door',
    'expectowner vladfood frank', 'expectset halla', 'expectscene Scene51',
    'expectview View58', 'expectflat main 1', 'expectglobal mission 1',
    'expectglobal phase 3', 'expectglobal playerdeath ', 'expectowner rubaiyat coal5'
) + $ExtraActions
$sashaChoices = @('sasha1.pup:101', 'sasha1.pup:102', 'sasha1.pup:101', 'sasha1.pup:102', 'sasha1.pup:103') + $Choices
& "$PSScriptRoot/verify-full-vlad.ps1" -ClickMovies $ClickMovies -Choices $sashaChoices -ExtraActions $sashaActions -TimeoutSeconds $TimeoutSeconds
