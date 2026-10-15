# SPDX-License-Identifier: GPL-3.0-or-later
# Place Leroy's earned matchbox in the schoolhouse drawer through inventory drag.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-matchbox.dfsv", [int]$TimeoutSeconds = 150)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan([string]$Room,[string]$From,[string]$Facing,[string]$To,[string]$Heading) {
    $route = @(& node "$PSScriptRoot/dust-route.mjs" $Room $From $Facing $To $Heading)
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return $route
}
$town = Plan TOWN K11 south G4 north
$court = Plan COURT C5 north C3 north
$school = Plan SCHOOL B2 north A2 north
$actions = @('expectowner matchbox stranger','expectglobal day 3','expectglobal phase 2') + $town + @(
    'click 250 150','expectowner door court','key uparrow','expectset court','expectscene Scene C5','expectview north'
) + $court + @('click 250 150','expectowner door schoolin','key uparrow','expectset school','expectscene Scene B2','expectview north') + $school + @(
    'click 250 220','idle 30','expectflat mainscorp','expectpropdeg thepages 1',
    'dragpoint prop matchbox 400 100','expectowner matchbox desk','clickon button exit',
    'expectflat mainpanel','expectset school','expectglobal playerdeath ','savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-matchbox-in-desk.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $actions
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-matchbox-in-desk-native.log"
