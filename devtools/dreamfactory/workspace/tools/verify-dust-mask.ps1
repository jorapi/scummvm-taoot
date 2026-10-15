# SPDX-License-Identifier: GPL-3.0-or-later
# Feed the dining-room bird with the apple and quietly collect the mask.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-mansion-entry.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan([string]$Room,[string]$From,[string]$Facing,[string]$To,[string]$Heading) {
    $route = @(& node "$PSScriptRoot/dust-route.mjs" $Room $From $Facing $To $Heading)
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return $route
}
$hall = Plan MAYHALL C4 north C3 east
$apple = Plan MAYDINE D2 east E3 north
$bird = Plan MAYDINE E3 north D3 west
$mask = Plan MAYDINE D3 west E3 east
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-mask.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions (@(
    'expectglobal day 3','expectglobal clock 2','expectowner flute stranger','expectowner yunnibook stranger'
) + $hall + @('click 250 150','expectowner door dine','key uparrow','expectset maydine',
    'expectscene Scene D2','expectview east'
) + $apple + @('clickon prop apple','expectowner apple stranger') + $bird + @(
    'dragto prop apple actor birdcage','expectowner apple birdcage'
) + $mask + @(
    'clickon prop mask','expectowner mask stranger','expectowner flute stranger','expectowner yunnibook stranger',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-mask-native.log"
