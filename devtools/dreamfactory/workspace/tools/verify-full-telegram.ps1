# SPDX-License-Identifier: GPL-3.0-or-later
# Mission two: collect Thayer's telegram, obtain wireless access, tune and send.
# Requires the native checkpoint produced by verify-full-mission1.ps1.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-mission2.dfsv", [int]$TimeoutSeconds = 600)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-TelegramRoute([string]$Room, [string]$From, [string]$To) {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    $route | ForEach-Object { ($_ -split ' # ')[0] }
}
function Invoke-TelegramStage([string]$Name, [string]$Restore, [string[]]$Actions, [string[]]$Choices = @(), [string[]]$MovieClicks = @()) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $Restore -Checkpoint "$root/build/save-full-$Name.dfsv" -Steps 1 -Actions $Actions -Choices $Choices -MovieClicks $MovieClicks -TimeoutSeconds $TimeoutSeconds
    Copy-Item "$root/build/boot-full.log" "$root/build/full-$Name-native.log"
}
Invoke-TelegramStage 'thayergram' $RestoreCheckpoint @(
    'expectglobal mission 2', 'expectglobal phase 0', 'expectview View76',
    'clickon prop map', 'key c', 'expectflat Map 4', 'clickon button Button41',
    'expectset gstair3', 'expectview View38', 'expectglobal savedeck c',
    'key rightarrow', 'key uparrow', 'key leftarrow', 'key leftarrow',
    'expectview View36', 'clickon painting door', 'key uparrow',
    'expectowner thayergram frank', 'expectactorowner purs sendgram', 'expectview View37', 'savecheckpoint'
) @('purs1.pup:102', 'purs1.pup:101', 'purs1.pup:101') @('mainc.mov:openit', 'mainc.mov:WIN 3')
$access = @(
    'clickon prop map', 'clickon button Button18', 'expectflat Map 1', 'clickon button Button27',
    'expectset gstair1', 'expectview View47', 'key leftarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'expectview View43', 'clickon painting door', 'key uparrow', 'expectset veststbd', 'expectview View18',
    'clickon painting door', 'key uparrow', 'expectset deckbd', 'expectview View110'
) + @(Get-TelegramRoute DECKBD View110 View102) + @(
    'expectview View102', 'clickon painting door', 'expectactorowner morrow enterwireless',
    'expectglobal morrowphase 3', 'savecheckpoint'
)
Invoke-TelegramStage 'wireless-access' "$root/build/save-full-thayergram.dfsv" $access @(
    'morrow1.pup:103', 'morrow1.pup:101', 'morrow1.pup:101', 'morrow1.pup:103',
    'morrow1.pup:103', 'morrow1.pup:102', 'morrow1.pup:999'
)
Invoke-TelegramStage 'wireless-panel' "$root/build/save-full-wireless-access.dfsv" @(
    'key rightarrow', 'key rightarrow', 'expectview View102', 'clickon painting door', 'key uparrow',
    'expectset wireless', 'expectview View14', 'clickon painting wireless', 'expectflat wireless 1', 'savecheckpoint'
)
Invoke-TelegramStage 'wireless-powered' "$root/build/save-full-wireless-panel.dfsv" @(
    'clickon prop senderhandle', 'expectflat wireless 4', 'dragpoint prop senderhandle 256 60',
    'expectowner senderhandle on', 'click 458 350', 'expectflat wireless 1',
    'clickon button breaker', 'expectflat wireless 2', 'dragpoint prop breakerhandle 50 150',
    'expectowner breakerhandle tx', 'click 458 350', 'expectflat wireless 1',
    'clickon button tuner', 'expectflat wireless 5', 'expectpropvalue tunerneedle 200', 'savecheckpoint'
)
Invoke-TelegramStage 'wireless-tuned' "$root/build/save-full-wireless-powered.dfsv" @(
    'dragarc prop tunerknob 333 119 -32 64', 'dragarcuntil prop tunerknob 333 119 -32 64 on',
    'expectowner tunerknob on', 'savecheckpoint'
)
Invoke-TelegramStage 'thayer-sent' "$root/build/save-full-wireless-tuned.dfsv" @(
    'expectowner tunerknob on', 'click 458 350', 'expectflat wireless 1',
    'clickon button tapper', 'expectflat wireless 3', 'key t', 'expectactorowner purs sentgram', 'savecheckpoint'
)
