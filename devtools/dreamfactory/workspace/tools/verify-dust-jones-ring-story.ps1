# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Jones's day-three request after collecting the schoolhouse pages.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-pages.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan([string]$Room,[string]$From,[string]$Facing,[string]$To,[string]$Heading) {
    $route = @(& node "$PSScriptRoot/dust-route.mjs" $Room $From $Facing $To $Heading)
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return $route
}
$school = Plan SCHOOL A2 north B2 south
$court = Plan COURT C3 south C5 south
$town = Plan TOWN G4 south G15 north
$actions = @('expectowner pages stranger','expectowner ring fear') + $school + @(
    'click 250 150','expectowner door schoolout','key uparrow','expectset court'
) + $court + @('click 250 150','expectowner door courtout','key uparrow','expectset town') + $town + @(
    'waitactorstar jones town.leroy1 60000','clickon actor jones','expectglobal jonesringstory 1',
    'expectglobal jonesphase 1','expectowner ring fear','expectglobal playerdeath ','savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-jones-ring-story.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'jones.pup:101','jones.pup:102','jones.pup:104','jones.pup:105'
) -Actions $actions
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-jones-ring-story-native.log"
