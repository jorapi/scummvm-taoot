# SPDX-License-Identifier: GPL-3.0-or-later
# Hear Fearwitt's warning, then Jones and Trotter before the Kid's attack.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day4-morning.dfsv", [int]$TimeoutSeconds = 360)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Plan($Room, $From, $View, $To, $Heading) {
    $r = @(& node "$PSScriptRoot/dust-route.mjs" $Room $From $View $To $Heading)
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    return $r
}
$a = @('expectglobal day 4','expectglobal clock 1','expectglobal phase 0')
$a += (Plan HOTROOM A1 east B1 east) + @('click 250 150','expectowner door inside','key uparrow','expectset hotupper')
$a += (Plan HOTUPPER C4 east D1 south) + @('key uparrow','expectset hotlower')
$a += (Plan HOTLOWER D3 south A2 north) + @('key uparrow','expectglobal fearphase 1','expectview east',
    'key rightarrow','key rightarrow','expectview west','click 250 150','expectowner door hotout','key uparrow','expectset town')
$a += (Plan TOWN G5 west G8 west) + @('click 270 100','expectowner door saloon','key uparrow','expectset sallower')
$a += (Plan SALLOWER D1 west D3 west) + @('clickon actor trotter','expectglobal trotterphase 1',
    'clickon actor jones','expectglobal jonesphase 1','clickon actor jones','expectglobal jonesphase 2',
    'expectglobal phase 1','expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-day4-saloon.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-day4-saloon-native.log"
