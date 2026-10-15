# SPDX-License-Identifier: GPL-3.0-or-later
# Open an account with a normal one-dollar bank deposit.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-laurel-bullets.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan([string]$Room,[string]$From,[string]$Facing,[string]$To,[string]$Heading) {
    $route = & node "$PSScriptRoot/dust-route.mjs" $Room $From $Facing $To $Heading
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return @($route)
}
$upstairs = Plan HOTUPPER C2 west D1 south
$lobby = Plan HOTLOWER D3 south A1 west
$town = Plan TOWN G5 west G6 west
$counter = Plan BANK D2 west D3 west
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-bank-deposit.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('teller.pup:101','teller.pup:101','teller.pup:104','teller.pup:105') -Actions (@(
    'expectglobal day 2','expectglobal clock 2','expectglobal phase 0',
    'expectowner bullets stranger','expectglobal playercash 5','expectglobal playeraccount 0'
) + $upstairs + @('key uparrow','expectset hotlower') + $lobby + @(
    'click 250 150','expectowner door hotout','key uparrow','expectset town'
) + $town + @(
    'click 250 150','expectowner door bank','key uparrow','expectset bank',
    'expectscene Scene D2','expectview west'
) + $counter + @(
    'click 250 150','expectglobal playeraccount 1','expectglobal playercash 4',
    'expectowner gun stranger','expectowner boots stranger','expectowner bullets stranger',
    'expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-bank-deposit-native.log"
