# SPDX-License-Identifier: GPL-3.0-or-later
# Leave the matchbox in the desk, return through town, and collect the safe pages.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-matchbox-in-desk.dfsv", [int]$TimeoutSeconds = 150)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan([string]$Room,[string]$From,[string]$Facing,[string]$To,[string]$Heading) {
    $route = @(& node "$PSScriptRoot/dust-route.mjs" $Room $From $Facing $To $Heading)
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return $route
}
$school = Plan SCHOOL A2 north B2 south
$court = Plan COURT C3 south C5 south
$backcourt = Plan COURT C5 north C3 north
$backschool = Plan SCHOOL B2 north A2 north
$actions = @('expectowner matchbox desk','expectowner pages none') + $school + @(
    'click 250 150','expectowner door schoolout','key uparrow','expectset court','expectscene Scene C3','expectview south'
) + $court + @(
    'click 250 150','expectowner door courtout','key uparrow','expectset town','expectscene Scene G4',
    'expectowner matchbox scorpion','key rightarrow','key rightarrow','expectview north',
    'click 250 150','expectowner door court','key uparrow','expectset court'
) + $backcourt + @('click 250 150','expectowner door schoolin','key uparrow','expectset school') + $backschool + @(
    'click 250 220','idle 30','expectflat mainscorp','expectpropdeg thematch 1','expectpropdeg thepages 0','savecheckpoint'
)
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-scorpion-trapped.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $actions
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-scorpion-trapped-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-scorpion-trapped.dfsv" -Checkpoint "$root/build/save-dust-pages.dfsv" -TimeoutSeconds 90 -Steps 1 -Actions @(
    'expectflat mainscorp','expectowner matchbox scorpion','expectpropdeg thepages 0',
    'clickon prop thematch','expectowner matchbox stranger','expectpropdeg matchbox 1',
    'clickon prop thepages','expectowner pages stranger','clickon button exit',
    'expectflat mainpanel','expectset school','expectglobal day 3','expectglobal clock 1',
    'expectglobal phase 2','expectglobal playerdeath ','savecheckpoint'
)
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-pages-native.log"
