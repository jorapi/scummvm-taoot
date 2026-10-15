# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Sonoma's final warning and approach the tower's stone box normally.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day4-night.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan($Room, $From, $View, $To, $Heading) {
    $r = @(& node "$PSScriptRoot/dust-route.mjs" $Room $From $View $To $Heading)
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return $r
}
$a = @('expectglobal day 4','expectglobal clock 3','expectglobal phase 0','expectowner tstone stranger')
$a += (Plan NITE G5 south G4 north) + @('click 250 100','expectowner door court','key uparrow','expectset court')
$a += (Plan NITECOUR C5 north B3 east) + @('clickon actor sonoma','expectglobal sonomaphase 1')
$a += (Plan NITECOUR B3 east C3 north) + @('click 250 150','expectowner door schoolin','key uparrow','expectset school')
$a += (Plan NITESCHO B2 north A2 west) + @('click 250 150','expectowner door padre','key uparrow','expectset padre')
$a += (Plan PADRE A2 west A3 east) + @('click 250 190','expectflat flat 0','expectowner tstone stranger',
    'expectglobal playerdeath ','targets','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-temple-box-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @('sonoma.pup:101') -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-temple-box-entry-native.log"
