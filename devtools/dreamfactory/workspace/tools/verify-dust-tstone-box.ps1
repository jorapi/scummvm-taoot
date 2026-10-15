# SPDX-License-Identifier: GPL-3.0-or-later
# Select the earned stone through the box's modal inventory and place it inside.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-temple-box-entry.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-tstone-box.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -ModalClicks @(
    'avatar:prop:tstone','avatar:button:ok'
) -Actions @('clickon button exit','expectflat mainpanel','click 250 190','expectflat flat 0',
    'clickon prop avatar','expectflat flat 0','expectglobal handitem tstone',
    'clickon button open','expectflat flat 1','dragpoint prop tstone 250 150',
    'expectowner tstone box','expectflat mainbox','clickon button close',
    'expectflat mainpanel','expectset padre','expectglobal day 4','expectglobal clock 3',
    'expectglobal playerdeath ','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-tstone-box-native.log"
