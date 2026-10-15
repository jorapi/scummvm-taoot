# SPDX-License-Identifier: GPL-3.0-or-later
# Second gang wave: exposed enemies and both rooftop death movies.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-kid-wave2-probe.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Route($From, $View, $To, $Heading) {
    $r = @(& node "$PSScriptRoot/dust-route.mjs" TOWN $From $View $To $Heading)
    if ($LASTEXITCODE) { throw 'Cannot plan gang approach' }
    return $r
}
function Shoot($Name, $Pose = '') {
    $r = @()
    1..4 | ForEach-Object {
        if ($Pose) { $r += "waitactorpose $Name $Pose 15000" }
        $r += @("clickon actor $Name", 'waitpropview gunhand idle 10000')
    }
    return $r + @('idle 10','expectglobal playerdeath ')
}
function Reload([int]$Count=4) {
    $r = @('clickon prop gunhand','expectpropview gunhand reload')
    for($bullet=0;$bullet -lt $Count;$bullet++) { $r += 'click 234 235' }
    return $r + @('expectglobal bulletcount 6','clickon prop gunhand','expectpropview gunhand idle')
}
$ammo=& python "$root/tools/read-checkpoint-globals.py" $RestoreCheckpoint bulletcount | ConvertFrom-Json
if($LASTEXITCODE -or $ammo.bulletcount -lt 0 -or $ammo.bulletcount -gt 6) { throw 'Cannot read second-wave ammunition' }
$a = @('expectglobal fightphase 2')
if($ammo.bulletcount -lt 6) { $a += (Reload (6-[int]$ammo.bulletcount)) }
$a += @('key rightarrow','key rightarrow','expectview north','waitactorpose kidgang3 lowstand 15000') + (Shoot kidgang3) + @('expectactorpose kidgang3 dead')
$a += (Reload) + (Route G11 north G13 east) + @('waitactorpose kidgang1 lowstand 15000') + (Shoot kidgang1) + @('expectactorpose kidgang1 dead')
$a += (Reload) + (Route G13 east J7 west) + (Shoot kidgang4) + @('expectactorpose kidgang4 dead')
$a += (Reload) + (Route J7 west A7 north) + @('waitactorpose kidgang2 lowstand 15000') + (Shoot kidgang2) + @('expectactorpose kidgang2 dead')
$a += (Reload) + (Route A7 north G5 north) + (Shoot kidgang5 standfire)
$a += @('expectglobal fightphase 3','expectglobal bulletcount 2','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-kid-wave3-entry.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-kid-wave2-complete-native.log"
