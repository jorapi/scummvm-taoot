# SPDX-License-Identifier: GPL-3.0-or-later
# Return to the hotel and sleep after earning both third-night artifacts.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-tbird.dfsv", [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan($Room, $From, $View, $To, $Heading) {
    $r = @(& node "$PSScriptRoot/dust-route.mjs" $Room $From $View $To $Heading)
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return $r
}
$a = @('expectglobal day 3','expectglobal clock 3','expectglobal phase 4',
    'expectowner tbird stranger','expectowner tstone stranger')
$a += (Plan SALLOWER C2 east D1 east) + @('click 250 150','expectowner door salout','key uparrow','expectset town')
$a += (Plan NITE G8 east G5 east) + @('click 250 150','expectowner door hotel','key uparrow','expectset hotlower')
$a += (Plan HOTLOWER A1 east D3 north) + @('key uparrow','expectset hotupper')
$a += (Plan HOTUPPER D1 north C1 west) + @('key leftarrow','expectglobal buickphase 2','expectview west')
$a += (Plan HOTUPPER C1 west C4 west) + @('click 250 150','expectowner door playroom','key uparrow','expectset hotroom')
$a += (Plan HOTROOM B1 west A1 west) + @('click 320 240',
    'expectglobal day 4','expectglobal clock 1','expectglobal phase 0',
    'expectset hotroom','expectscene Scene A1','expectview east',
    'expectowner tbird stranger','expectowner tstone stranger','expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day4-morning.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Choices @(
    'buick.pup:101','buick.pup:103','buick.pup:104'
) -MovieClicks @(
    'hotbed.mov:#1','hotbed.mov:#1'
) -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-third-night-native.log"
