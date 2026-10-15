# Deliver Sasha's package to Vlad, then recover the safely hidden Rubaiyat.
param([string[]]$BeforeDeliveryActions = @(), [string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
function Get-PackageRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
$packageActions = $BeforeDeliveryActions + @(
    'clickon prop map', 'key f', 'expectflat Map 7', 'clickon button Button22',
    'expectset stair2c', 'expectview View28', 'clickon painting door',
    'key uparrow', 'expectset hallf2c', 'expectview View70'
) + @(Get-PackageRoute HALLF2C View70 View13) + @(
    'clickon painting door', 'key uparrow', 'expectset turb', 'expectview View89'
) + @(Get-PackageRoute TURB View89 View74) + @(
    'clickon painting door', 'key uparrow', 'expectset control', 'expectview View13'
) + @(Get-PackageRoute CONTROL View13 View32) + @(
    'clickon painting door', 'key uparrow', 'expectset engine', 'expectview View66'
) + @(Get-PackageRoute ENGINE View66 View70) + @(
    'clickon painting door', 'key uparrow', 'expectset boil',
    'expectview View46', 'expectglobal savedeck boil1'
) + @(Get-PackageRoute BOIL View46 View44) + @(
    'clickon painting door', 'key uparrow', 'expectglobal savedeck boil2', 'expectview View45'
) + @(Get-PackageRoute BOIL View45 View47) + @(
    'clickon painting door', 'key uparrow', 'expectglobal savedeck boil3', 'expectview View46'
) + @(Get-PackageRoute BOIL View46 View45) + @(
    'expectscene Scene40', 'expectview View45', 'expectowner rubaiyat coal5',
    'waitactorwalk vlad 0', 'clickon actor vlad', 'expectowner vladfood vlad',
    'expectflat main 1', 'expectglobal phase 3', 'expectglobal playerdeath '
) + @(Get-PackageRoute BOIL View45 View15) + @(
    'expectscene Scene14', 'expectview View15', 'clickon painting coal5',
    'expectflat boil 1', 'clickon prop boildoor', 'expectpropview boildoor idleopen',
    'clickon prop boilrubaiyat', 'expectowner rubaiyat frank', 'expectglobal phase 4',
    'clickon button ok', 'expectflat main 1', 'expectglobal mission 1',
    'expectowner vladfood vlad', 'expectglobal playerdeath '
) + $ExtraActions
$packageChoices = @('vlad1.pup:101') + $Choices
& "$PSScriptRoot/verify-full-sasha.ps1" -ClickMovies $ClickMovies -Choices $packageChoices -ExtraActions $packageActions -TimeoutSeconds $TimeoutSeconds
