# SPDX-License-Identifier: GPL-3.0-or-later
# Deliberately approach Vlad while carrying the Rubaiyat, through normal input.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-sasha.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Route([string]$Room, [string]$From, [string]$To) {
    $r = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    @($r | ForEach-Object { ($_ -split ' # ')[0] })
}
$actions = @('expectglobal mission 1','expectglobal phase 3','expectowner rubaiyat coal5',
    'clickon prop map','key f','expectflat Map 7','clickon button Button22',
    'expectset stair2c','expectview View28','clickon painting door','key uparrow',
    'expectset hallf2c','expectview View70') + @(Route HALLF2C View70 View13) + @(
    'clickon painting door','key uparrow','expectset turb','expectview View89'
) + @(Route TURB View89 View74) + @(
    'clickon painting door','key uparrow','expectset control','expectview View13'
) + @(Route CONTROL View13 View32) + @(
    'clickon painting door','key uparrow','expectset engine','expectview View66'
) + @(Route ENGINE View66 View70) + @(
    'clickon painting door','key uparrow','expectset boil','expectview View46','expectglobal savedeck boil1'
) + @(Route BOIL View46 View44) + @(
    'clickon painting door','key uparrow','expectglobal savedeck boil2','expectview View45'
) + @(Route BOIL View45 View47) + @(
    'clickon painting door','key uparrow','expectglobal savedeck boil3','expectview View46'
) + @(Route BOIL View46 View15) + @(
    'clickon painting coal5','expectflat boil 1','clickon prop boildoor','expectpropview boildoor idleopen',
    'clickon prop boilrubaiyat','expectowner rubaiyat frank','clickon button ok','expectflat main 1'
) + @(Route BOIL View15 View39) + @(
    'expectglobal mission 1',
    'expectset none','expectflat ctl 1','expectglobal playerdeath by vlad','targets','savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-vlad-death.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions $actions
Copy-Item "$root/build/boot-full.log" "$root/build/full-vlad-death-native.log"
Write-Output 'Titanic carrying-Rubaiyat death and recovery menu PASS'
