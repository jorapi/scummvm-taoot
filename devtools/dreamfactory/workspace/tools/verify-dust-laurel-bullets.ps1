# SPDX-License-Identifier: GPL-3.0-or-later
# Trade Bloodstone's skooch to Laurel for ammunition.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-blood-skooch.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" HOTUPPER C4 east C2 west
if ($LASTEXITCODE) { throw 'Cannot plan Laurel approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-laurel-bullets.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('laurel.pup:55555','laurel.pup:55555') -ModalClicks @('avatar:prop:hankerchief','avatar:button:ok') -Actions (@(
    'expectglobal day 2','expectglobal clock 2','expectpropdeg hankerchief 4'
) + @($route) + @(
    'click 250 150','expectowner hankerchief laurel','expectowner bullets stranger',
    'expectglobal laurelgood 1','expectglobal laurelphase 2','expectglobal playerdeath ',
    'savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-laurel-bullets-native.log"
