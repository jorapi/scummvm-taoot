# SPDX-License-Identifier: GPL-3.0-or-later
# The tower stone opens the fountain passage into the final puzzle hub.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-tstone-box.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan($Room, $From, $View, $To, $Heading) {
    $r = @(& node "$PSScriptRoot/dust-route.mjs" $Room $From $View $To $Heading)
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return $r
}
$a = @('expectowner tstone box','expectglobal day 4','expectglobal clock 3')
$a += (Plan PADRE A3 east A2 east) + @('click 250 150','expectowner door padreout','key uparrow','expectset school')
$a += (Plan NITESCHO A2 east B2 south) + @('click 250 150','expectowner door schoolout','key uparrow','expectset court')
$a += (Plan NITECOUR C3 south B4 east) + @('click 250 220','expectset hub',
    'expectscene Scene D7','expectview north','expectowner tbird stranger',
    'expectowner flute stranger','expectowner mask stranger','expectowner yunnibook stranger',
    'expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-hub-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-hub-entry-native.log"
