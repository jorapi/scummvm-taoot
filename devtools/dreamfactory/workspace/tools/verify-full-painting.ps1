# SPDX-License-Identifier: GPL-3.0-or-later
# Mission two: report the telegram, inspect the manifest, recover keys and painting.
# Run verify-full-telegram.ps1 first, or supply an equivalent native checkpoint.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-thayer-sent.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-PaintingRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
function Invoke-PaintingStage([string]$Name, [string]$Restore, [string[]]$Actions, [string[]]$Choices = @(), [string[]]$MovieClicks = @()) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $Restore -Checkpoint "$root/build/save-full-$Name.dfsv" -Steps 1 -Actions $Actions -Choices $Choices -MovieClicks $MovieClicks -TimeoutSeconds $TimeoutSeconds
    Copy-Item "$root/build/boot-full.log" "$root/build/full-$Name-native.log"
}
$purserRoute = @(
    'clickon prop map', 'key c', 'expectflat Map 4', 'clickon button Button41',
    'expectset gstair3', 'expectview View38', 'key rightarrow', 'key uparrow',
    'key leftarrow', 'key leftarrow', 'expectview View36', 'clickon painting door', 'key uparrow'
)
Invoke-PaintingStage 'manifest' $RestoreCheckpoint (@(
    'expectactorowner purs sentgram', 'click 457 351', 'expectflat wireless 1',
    'click 457 351', 'expectflat main 1', 'key rightarrow', 'key rightarrow', 'expectview View13',
    'clickon painting door', 'key uparrow', 'expectset deckbd'
) + $purserRoute + @('expectactorowner purs none2', 'expectview View37', 'savecheckpoint')) @(
    'purs1.pup:102', 'purs1.pup:101'
) @('mainc.mov:openit', 'maino1.mov:blackframe', 'man.mov:frame3', 'man.mov:frame5', 'man.mov:frame6', 'maino1.mov:WIN 3')
Invoke-PaintingStage 'cufflink' "$root/build/save-full-manifest.dfsv" (@(
    'key leftarrow', 'key leftarrow', 'expectview View36', 'clickon painting door', 'key uparrow',
    'expectactorowner purs findcuff', 'expectview View37', 'clickon prop map', 'key d',
    'expectflat Map 5', 'clickon button Button42', 'expectset recept1c', 'expectview View27'
) + @(Get-PaintingRoute RECEPT1C View27 View25) + @(
    'expectview View25', 'clickon painting cufflink1', 'expectflat cuff 1',
    'clickon prop cuffcuff', 'expectpropview cuffcuff med', 'clickon prop cuffcuff',
    'expectpropview cuffcuff big', 'clickon prop cuffcuff', 'expectowner cufflink frank',
    'expectactorowner purs foundcuff', 'savecheckpoint'
)) @('purs1.pup:102', 'purs1.pup:102') @('mainc.mov:openit', 'mainc.mov:WIN 3')
Invoke-PaintingStage 'carkeys' "$root/build/save-full-cufflink.dfsv" (@(
    'click 457 351', 'expectflat main 1'
) + $purserRoute + @(
    'expectowner cufflink purser', 'expectowner carkeys frank',
    'expectactorowner purs left2', 'expectview View37', 'savecheckpoint'
)) @('purs1.pup:102') @('mainc.mov:openit', 'maino2.mov:key.mov', 'key.mov:keys.SE', 'key.mov:#1')
Invoke-PaintingStage 'cargo-access' "$root/build/save-full-carkeys.dfsv" (@(
    'clickon prop map', 'key a', 'expectflat Map 2', 'clickon button Button32',
    'expectset stair1c1', 'expectview View74'
) + @(Get-PaintingRoute STAIR1C1 View74 View75) + @(
    'clickon painting door', 'key uparrow', 'expectset decka', 'expectview View402'
) + @(Get-PaintingRoute DECKA View402 View455) + @(
    'key uparrow', 'expectset fore', 'expectview View491'
) + @(Get-PaintingRoute FORE View491 View594) + @(
    'clickon painting door', 'key uparrow', 'expectset crew', 'expectview View16'
) + @(Get-PaintingRoute CREW View16 View14) + @(
    'clickon painting door', 'key uparrow', 'expectset carghall', 'expectview View12'
) + @(Get-PaintingRoute CARGHALL View12 View20) + @(
    'clickon painting door', 'expectglobal bseaphase 1', 'expectset cargo', 'expectview View60', 'savecheckpoint'
))
Invoke-PaintingStage 'cargo-star' "$root/build/save-full-cargo-access.dfsv" (@(
    Get-PaintingRoute CARGO View60 View18
) + @('expectview View18', 'key uparrow', 'expectglobal hallside star', 'savecheckpoint'))
Invoke-PaintingStage 'painting' "$root/build/save-full-cargo-star.dfsv" (@(
    'expectglobal hallside star'
) + @(Get-PaintingRoute CARGO View18 View16) + @(
    'clickon painting door', 'key uparrow', 'expectset bind', 'expectview View15'
) + @(Get-PaintingRoute BIND View15 View16) + @(
    'clickon painting car', 'expectglobal carlights 1', 'expectset binl', 'expectview View16'
) + @(Get-PaintingRoute BINL View16 View17) + @(
    'clickon painting crate', 'expectflat cargo 1', 'clickon prop cargopainting',
    'expectowner painting frank', 'expectglobal mission 2', 'expectglobal phase 1',
    'expectowner carkeys xxxfrank', 'savecheckpoint'
)) @() @('lightoff.mov:switch.SE', 'lightoff.mov:#1')
