# SPDX-License-Identifier: GPL-3.0-or-later
# Exchange Buick's wine with Bloodstone through the conversation inventory.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-buick-wine.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" HOTUPPER C1 west C4 east
if ($LASTEXITCODE) { throw 'Cannot plan Bloodstone approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-blood-skooch.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('blood.pup:55555','blood.pup:55555','blood.pup:105') -ModalClicks @('avatar:prop:hankerchief','avatar:button:ok') -Actions (@(
    'expectglobal day 2','expectglobal clock 2','expectpropdeg hankerchief 3'
) + @($route) + @(
    'click 250 150','expectowner hankerchief stranger','expectpropdeg hankerchief 4',
    'expectglobal bloodphase 1','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-blood-skooch-native.log"
