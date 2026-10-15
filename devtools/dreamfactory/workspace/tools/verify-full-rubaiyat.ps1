# Reach boiler room three and recover the Rubaiyat from its coal chute.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
function Get-RoomActions([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
$rubaiyatActions = @(Get-RoomActions CONTROL View24 View32) + @(
    'expectview View32', 'clickon painting door', 'key uparrow',
    'expectset engine', 'expectview View66'
) + @(Get-RoomActions ENGINE View66 View70) + @(
    'expectview View70', 'clickon painting door', 'key uparrow',
    'expectset boil', 'expectview View46', 'expectglobal savedeck boil1'
) + @(Get-RoomActions BOIL View46 View44) + @(
    'expectview View44', 'clickon painting door', 'key uparrow',
    'expectglobal savedeck boil2', 'expectview View45'
) + @(Get-RoomActions BOIL View45 View47) + @(
    'expectview View47', 'clickon painting door', 'key uparrow',
    'expectglobal savedeck boil3', 'expectview View46'
) + @(Get-RoomActions BOIL View46 View21) + @(
    'expectview View21', 'clickon painting coal4', 'expectflat boil 1',
    'clickon prop boildoor', 'expectpropview boildoor idleopen',
    'clickon prop boilrubaiyat', 'expectowner rubaiyat frank',
    'clickon button ok', 'expectflat main 1', 'expectglobal phase 3'
) + $ExtraActions
& "$PSScriptRoot/verify-full-turbine.ps1" -ClickMovies $ClickMovies -Choices $Choices -ExtraActions $rubaiyatActions -TimeoutSeconds $TimeoutSeconds
