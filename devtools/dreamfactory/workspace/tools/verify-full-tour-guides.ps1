# SPDX-License-Identifier: GPL-3.0-or-later
# Play the three guide movies present in the supplied retail package.
param([ValidateSet('start','burns','trask')][string]$StartAt = 'start', [string]$RestoreCheckpoint = '', [int]$TimeoutSeconds = 600)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Get-TourRoute([string]$Room, [string]$From, [string]$To, [string]$Volume = '2') {
    $route = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To $Volume
    if ($LASTEXITCODE) { throw "Cannot plan $Room tour route" }
    @($route | ForEach-Object { ($_ -split ' # ')[0] })
}
function Play-Guide([string]$Name, [string]$Movie, [string]$Source, [string[]]$Actions, [string[]]$Choices = @(), [string[]]$MovieClicks = @()) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $Source -Checkpoint "$root/build/save-full-tour-$Name-finished.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions ($Actions + @('expectglobal tour 1','expectflat main 1','savecheckpoint')) -Choices $Choices -MovieClicks $MovieClicks
    Copy-Item "$root/build/boot-full.log" "$root/build/full-tour-$Name-native.log"
    $log = Get-Content "$root/build/boot-full.log" -Raw
    if ($log -notmatch "MOVIE PLAY $([regex]::Escape($Movie)) " -or $log -notmatch 'BOOT READY') { throw "$Name did not play its supplied tour movie and return" }
}
if (!$RestoreCheckpoint) {
    $RestoreCheckpoint = switch ($StartAt) {
        start { "$root/build/save-full-tour-entry.dfsv" }
        burns { "$root/build/save-full-tour-smeth-finished.dfsv" }
        trask { "$root/build/save-full-tour-burns-finished.dfsv" }
    }
}
$current = $RestoreCheckpoint
if ($StartAt -eq 'start') {
    Play-Guide smeth tour9.mov $current (@('expectset c73','expectview View63','key uparrow','expectview View54') + @(Get-TourRoute C73 View54 View55 1) + @('clickon actor smeth','expectset c73','expectview View55')) @('smeth1.pup:102') @('tour9.mov:#1')
    $current = "$root/build/save-full-tour-smeth-finished.dfsv"
}
if ($StartAt -ne 'trask') {
    Play-Guide burns tour2.mov $current (@('clickon prop map','clickon prop map','expectflat Map 4','key a','expectflat Map 2','clickon button Button32','expectset stair1c1','expectview View74') + @(Get-TourRoute STAIR1C1 View74 View94) + @('clickon painting door','key uparrow','expectset smoke','expectview View33') + @(Get-TourRoute SMOKE View33 View60) + @('clickon actor burns','expectset smoke','expectview View60'))
    $current = "$root/build/save-full-tour-burns-finished.dfsv"
}
Play-Guide trask tour8.mov $current (@('clickon prop map','key f','expectflat Map 7','clickon button Button44','expectset turkstrs','expectview View16','clickon painting door','key uparrow','expectset turk','expectview View116') + @(Get-TourRoute TURK View116 View119) + @('clickon actor trask','expectset turk','expectview View119'))
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-tour-trask-finished.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @('expectglobal tour 1','expectset turk','expectview View119','key rightarrow','key leftarrow','expectview View119')
Copy-Item "$root/build/boot-full.log" "$root/build/full-tour-guides-restore-native.log"
Write-Output 'Supplied Titanic tour guide movies, historical narration and restored navigation PASS'
