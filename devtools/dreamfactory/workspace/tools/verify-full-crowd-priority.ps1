# SPDX-License-Identifier: GPL-3.0-or-later
# Real sinking-deck crowd loops, original navigation and unmodified saves.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-best-sinking-phase1.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Route([string]$From, [string]$To) {
    $actions = & node "$PSScriptRoot/room-route.mjs" full DECKBD2 $From $To 1
    if ($LASTEXITCODE) { throw 'Cannot plan crowd-deck route' }
    @($actions | ForEach-Object { ($_ -split ' # ')[0] })
}
function Trace-Crowd([string]$Name, [string]$Source, [string[]]$Actions) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $Source -Checkpoint "$root/build/save-full-crowd-$Name.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions ($Actions + @('savecheckpoint'))
    Copy-Item "$root/build/boot-full.log" "$root/build/full-crowd-$Name-native.log"
}
Trace-Crowd 'priority' $RestoreCheckpoint @(
    'expectglobal mission 4','expectglobal phase 1','expectset deckbd2','expectview View250',
    'idle 3','expectambient party1 1 139 88','expectambient party2 1 67 104',
    'expectambient party3 0 0 0'
)
Trace-Crowd 'priority-promoted' "$root/build/save-full-crowd-priority.dfsv" (@(
    'expectambient party1 1 139 88','expectambient party2 1 67 104','expectambient party3 0 0 0'
) + @(Route View250 View233) + @(
    'expectscene Scene49','idle 3','expectambient party1 0 0 0','expectambient party2 0 0 0',
    'expectambient party3 1 158 -30','expectambient party4 1 109 -74'
))
Trace-Crowd 'priority-restored' "$root/build/save-full-crowd-priority-promoted.dfsv" @(
    'expectview View233','expectambient party1 0 0 0','expectambient party2 0 0 0',
    'expectambient party3 1 158 -30','expectambient party4 1 109 -74',
    'key rightarrow','key leftarrow','expectview View233','idle 3',
    'expectambient party3 1 158 -30','expectambient party4 1 109 -74'
)
Write-Output 'Titanic crowd priority from three/four audible candidates, movement promotion, fresh restores and navigation PASS'
