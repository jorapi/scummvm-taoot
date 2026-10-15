# SPDX-License-Identifier: GPL-3.0-or-later
# Climb to the player's room and choose sleep in the authored movie.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-night-wine.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan-Route([string]$Room,[string]$From,[string]$Facing,[string]$To,[string]$Heading) {
    $route = & node "$PSScriptRoot/dust-route.mjs" $Room $From $Facing $To $Heading
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return @($route)
}
$lower = Plan-Route HOTLOWER A2 east D3 north
$approach = Plan-Route HOTUPPER D1 north C1 west
$upper = Plan-Route HOTUPPER C1 west C4 west
$bed = Plan-Route HOTROOM B1 west A1 west
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day2-morning.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('buick.pup:104') -MovieClicks @('hotbed.mov:#1','hotbed.mov:#1') -Actions (@(
    'expectglobal day 1','expectglobal phase 9','expectowner hrkey stranger'
) + $lower + @(
    'key uparrow','expectset hotupper','expectscene Scene D1','expectview north'
) + $approach + @(
    'key leftarrow','expectview west','expectglobal buickphase 1'
) + $upper + @(
    'click 250 150','expectowner door playroom','key uparrow',
    'expectset hotroom','expectscene Scene B1','expectview west'
) + $bed + @(
    'click 320 240','expectglobal day 2','expectglobal clock 1','expectglobal phase 0',
    'expectset hotroom','expectscene Scene A1','expectview east',
    'expectowner hrkey stranger','expectowner ring fear','expectglobal playercash 6',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-first-night-native.log"
