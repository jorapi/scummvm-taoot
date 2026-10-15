# SPDX-License-Identifier: GPL-3.0-or-later
# Final gang wave: conserve health at range, then use two explosive barrels.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-kid-wave3-entry.dfsv", [int]$TimeoutSeconds = 180)
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
    return $r + @('idle 10',"expectactorpose $Name dead",'expectglobal playerdeath ')
}
function Reload($Count) {
    $r = @('clickon prop gunhand','expectpropview gunhand reload')
    1..$Count | ForEach-Object { $r += 'click 234 235' }
    return $r + @('expectglobal bulletcount 6','clickon prop gunhand','expectpropview gunhand idle')
}
$a = @('expectglobal fightphase 3','expectglobal bulletcount 2') + (Reload 4)
$a += (Route G5 north G15 north) + @('clickon prop powderkeg1','waitpropview gunhand idle 10000',
    'waitpropview powderkeg1 burned 10000','expectactorpose kidgang2 dead')
$a += (Route G15 north G13 north) + (Shoot kidgang1) + (Reload 5)
$a += @('key rightarrow','expectview east','idle 15','key leftarrow','expectview north')
$a += (Route G13 north G6 north) + @('clickon prop powderkeg3','waitpropview gunhand idle 10000',
    'idle 10','expectactorpose kidgang4 dead','expectactorpose kidgang5 dead')
$a += (Shoot kidgang3) + @('expectglobal bulletcount 1','expectglobal fightphase 3','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-kid-waves-defeated.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-kid-wave3-complete-native.log"
