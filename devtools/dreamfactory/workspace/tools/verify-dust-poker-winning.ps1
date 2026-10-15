# SPDX-License-Identifier: GPL-3.0-or-later
# Replay earned hands with real clicks; never alter cash, cards or game globals.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-poker-six-dollar-ready.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Test-Hand([string]$Name, [string]$Source, [string[]]$Choices, [string[]]$Actions, [string[]]$ModalClicks = @()) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $Source -Checkpoint "$root/build/save-dust-poker-$Name.dfsv" -TimeoutSeconds $TimeoutSeconds -Choices $Choices -ModalClicks $ModalClicks -Actions ($Actions + @('savecheckpoint'))
    Copy-Item "$root/build/boot-dust.log" "$root/build/dust-poker-$Name-native.log"
}
Test-Hand 'six-dollar-folded' $RestoreCheckpoint @('mez.pup:104','mez.pup:103') @(
    'clickon prop continue','expectglobal playerphase 4','expectglobal winner mez','expectglobal playercash 5'
) @('flat 0:prop:continue:viewcards')
Test-Hand 'repeat-ready' "$root/build/save-dust-poker-six-dollar-folded.dfsv" @('mez.pup:101') @(
    'clickon prop continue','expectflat flat 0','expectglobal playerphase 1','expectglobal playercash 5'
)
Test-Hand 'repeat-bet1' "$root/build/save-dust-poker-repeat-ready.dfsv" @('mez.pup:101') @(
    'clickon prop continue','expectglobal playerphase 2'
)
Test-Hand 'repeat-drawn' "$root/build/save-dust-poker-repeat-bet1.dfsv" @() @(
    'clickon prop 3d','clickon prop 7d','clickon prop 8c','expectglobal playercount 2',
    'clickon prop continue','expectglobal playerphase 3'
)
Test-Hand 'repeat-won' "$root/build/save-dust-poker-repeat-drawn.dfsv" @('mez.pup:101') @(
    'clickon prop continue','expectglobal playerphase 4','expectglobal winner player','expectglobalge playercash 6'
)
Test-Hand 'won-return' "$root/build/save-dust-poker-repeat-won.dfsv" @('mez.pup:102','mez.pup:102') @(
    'clickon prop continue','expectflat mainpanel','expectglobal hasquitpoker 1',
    'expectowner cards stranger','expectglobalge playercash 6',
    'key rightarrow','expectview south','key leftarrow','expectview east'
)
Write-Output 'Dust poker View Cards, fold, repeat deal, three discards, win, payout and exit PASS'
