# SPDX-License-Identifier: GPL-3.0-or-later
# First gang wave: three moving enemies and two native ammunition reloads.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-kid-fight-active.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Route($From, $View, $To, $Heading) {
    $r = @(& node "$PSScriptRoot/dust-route.mjs" TOWN $From $View $To $Heading)
    if ($LASTEXITCODE) { throw 'Cannot plan gang approach' }
    return $r
}
function Shoot($Name) {
    $r = @()
    1..4 | ForEach-Object { $r += @("clickon actor $Name", 'waitpropview gunhand idle 10000') }
    return $r + @('idle 10', "expectactorpose $Name dead", 'expectglobal playerdeath ')
}
function Reload {
    $r = @('clickon prop gunhand','expectpropview gunhand reload')
    1..4 | ForEach-Object { $r += 'click 234 235' }
    return $r + @('expectglobal bulletcount 6','clickon prop gunhand','expectpropview gunhand idle')
}
$a = @('expectglobal fightphase 1','expectglobal bulletcount 6')
$a += (Route G8 east C7 west) + (Shoot kidgang1)
$a += (Reload) + (Route C7 west chicken west) + (Shoot kidgang3)
$a += (Reload) + (Route chicken west H7 east) + (Shoot kidgang2)
$a += @('expectglobal fightphase 1','expectglobal bulletcount 2','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-kid-wave1-defeated.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-kid-wave1-complete-native.log"
