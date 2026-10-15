# SPDX-License-Identifier: GPL-3.0-or-later
# Redeem Fearwitt's chit with Buick, choosing the wine rather than the fur.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-handkerchief-chit.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$lower = & node "$PSScriptRoot/dust-route.mjs" HOTLOWER A1 east D3 north
if ($LASTEXITCODE) { throw 'Cannot plan hotel stairs' }
$upper = & node "$PSScriptRoot/dust-route.mjs" HOTUPPER D1 north C1 west
if ($LASTEXITCODE) { throw 'Cannot plan Buick approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-buick-wine.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('buick.pup:55555','buick.pup:55555','buick.pup:101','buick.pup:103') -ModalClicks @('avatar:prop:hankerchief','avatar:button:ok') -Actions (@(
    'expectglobal day 2','expectglobal clock 2','expectpropdeg hankerchief 1',
    'expectglobal handitem hankerchief','expectglobal handflag 1'
) + @($lower) + @(
    'key uparrow','expectset hotupper','expectscene Scene D1','expectview north'
) + @($upper) + @(
    'click 250 150','expectpropdeg hankerchief 3','expectowner hankerchief stranger',
    'expectglobal buickphase 2','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-buick-wine-native.log"
