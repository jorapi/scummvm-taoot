# SPDX-License-Identifier: GPL-3.0-or-later
# Sleep in the player's hotel room and reach the third morning normally.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-night2-laurel.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan([string]$Room,[string]$From,[string]$Facing,[string]$To,[string]$Heading) {
    $route = & node "$PSScriptRoot/dust-route.mjs" $Room $From $Facing $To $Heading
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return @($route)
}
$lower = Plan HOTLOWER B2 south D3 north
$upper = Plan HOTUPPER D1 north C4 west
$bed = Plan HOTROOM B1 west A1 west
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day3-morning.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -MovieClicks @('hotbed.mov:#1','hotbed.mov:#1') -Actions (@(
    'expectglobal day 2','expectglobal clock 3','expectglobal phase 1','expectglobal laurelgood 1'
) + $lower + @('key uparrow','expectset hotupper','expectscene Scene D1','expectview north') + $upper + @(
    'click 250 150','expectowner door playroom','key uparrow','expectset hotroom',
    'expectscene Scene B1','expectview west'
) + $bed + @(
    'click 320 240','expectglobal day 3','expectglobal clock 1','expectglobal phase 0',
    'expectset hotroom','expectscene Scene A1','expectview east','expectowner ring fear',
    'expectowner badge stranger','expectglobal playercash 4','expectglobal playeraccount 1',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-second-night-native.log"
