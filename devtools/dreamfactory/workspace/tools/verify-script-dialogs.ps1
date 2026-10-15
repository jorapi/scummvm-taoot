# SPDX-License-Identifier: GPL-3.0-or-later
# Exercise the authored Quit controls and real modal GUI keys, then restore saves.
param([int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Invoke-DialogTrace([string]$Edition, [string]$Name, [string[]]$Actions, [string[]]$Answers, [int]$Slot = -1) {
    $restore = switch ($Edition) {
        dust { "$root/build/save-dust-day5-morning.dfsv" }
        full { "$root/build/save-full-opening.dfsv" }
        demo { "$root/build/save-demo-opening.dfsv" }
    }
    $clickMovies = if ($Edition -eq 'demo') { @('close.mov') } else { @() }
    & "$PSScriptRoot/trace-boot.ps1" -Editions $Edition -RestoreCheckpoint $restore -Actions $Actions -DialogAnswers $Answers -DialogSlot $Slot -ClickMovies $clickMovies -TimeoutSeconds $TimeoutSeconds
    Copy-Item "$root/build/boot-$Edition.log" "$root/build/dialog-$Edition-$Name-native.log"
}
Invoke-DialogTrace dust cancel @('clickon button horn','expectflat score','click 252 259','expectflat score',
    'clickon button OK','expectflat mainpanel','key rightarrow','expectview west') @('no')
Invoke-DialogTrace full cancel @('clickon prop life','clickon prop life','expectflat ctl 1','click 130 160',
    'expectflat ctl 1','click 130 328','expectflat main 1','key rightarrow','expectview View16') @('no')
Invoke-DialogTrace demo cancel @('clickon prop life','expectflat ctl 1','click 130 160',
    'expectflat ctl 1','click 130 328','expectflat main 1','key rightarrow') @('no')
Invoke-DialogTrace dust quit @('clickon button horn','expectflat score','clickquit 252 259') @('yes','no')
Invoke-DialogTrace full quit @('clickon prop life','clickon prop life','expectflat ctl 1','clickquit 130 160') @('yes','no')
# Demo's in-game Quit returns to its own menu. That menu's Quit plays close.mov.
Invoke-DialogTrace demo quit @('clickon prop life','expectflat ctl 1','click 130 160','expectset none',
    'expectflat demo 1','click 256 340','expectflat demo 1','clickquit 256 340') @('yes','no','no','yes')
Invoke-DialogTrace dust savequit @('clickon button horn','expectflat score','clickquit 252 259') @('yes','yes') 96
Invoke-DialogTrace full savequit @('clickon prop life','clickon prop life','expectflat ctl 1','clickquit 130 160') @('yes','yes') 97
Invoke-DialogTrace demo savequit @('clickon prop life','expectflat ctl 1','click 130 160','expectset none',
    'expectflat demo 1','clickquit 256 340') @('yes','yes','yes') 98
Invoke-DialogTrace dust savedrestore @('loadslot 96','expectglobal day 5','expectowner chest stranger',
    'expectflat score','clickon button OK','expectflat mainpanel','key rightarrow','expectview west') @()
Invoke-DialogTrace full savedrestore @('loadslot 97','expectflat main 1','expectview View14',
    'key rightarrow','expectview View16') @()
Invoke-DialogTrace demo savedrestore @('loadslot 98','expectset c71','expectflat main 1','expectview View55',
    'key rightarrow') @()
Write-Output 'Native script confirmation, cancellation, quit and save-before-quit PASS'
