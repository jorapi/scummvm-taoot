# SPDX-License-Identifier: GPL-3.0-or-later
# Native combat: five hunters, ammunition reloads and the post-fight unlock.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-bounty-active.dfsv", [int]$TimeoutSeconds = 360)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Route($From, $View, $To, $Heading) {
    $r = @(& node "$PSScriptRoot/dust-route.mjs" NITE $From $View $To $Heading)
    if ($LASTEXITCODE) { throw 'Cannot plan combat route' }
    return $r
}
function Shoot($Name) {
    $r = @()
    1..4 | ForEach-Object { $r += @("clickon actor $Name", 'waitpropview gunhand idle 10000') }
    return $r + @('idle 10', "expectactorpose $Name dead", 'expectglobal playerdeath ')
}
function Reload($Count) {
    $r = @('clickon prop gunhand','expectpropview gunhand reload')
    1..$Count | ForEach-Object { $r += 'click 234 235' }
    return $r + @('expectglobal bulletcount 6','clickon prop gunhand','expectpropview gunhand idle')
}
$a = @('expectglobal fighton 1','expectglobal phase 3',
    'clickon prop avatar','expectflat avatar','clickon prop gun','clickon button ok',
    'expectglobal handitem gun','clickon prop gun','waitpropview gunhand idle 10000')
$a += (Route G11 north G13 west) + (Shoot bounty4)
$a += (Reload 5) + @('key leftarrow','expectview south') + (Shoot bounty1)
$a += (Reload 4) + (Route G13 south G9 north) + (Shoot bounty5)
$a += (Reload 4) + (Route G9 north I7 east) + (Shoot bounty2)
$a += (Reload 4) + (Route I7 east B11 north) + (Shoot bounty3)
$a += @('clickon prop gun','idle 10') + (Route B11 north G9 south) + @(
    'idle 30','key rightarrow','key rightarrow','expectview north','clickon actor trotter',
    'expectglobal fighton 0','expectglobal trotterphase 1','expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-bounty-trotter.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'trotter.pup:101','trotter.pup:102','trotter.pup:103'
) -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-bounty-complete-native.log"
