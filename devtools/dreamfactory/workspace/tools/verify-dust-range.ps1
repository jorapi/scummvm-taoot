# SPDX-License-Identifier: GPL-3.0-or-later
# Physical tower collision, six breakable targets, reload and Leroy's reward.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day3-range-entry.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$actions = @('expectset target','expectglobal bulletcount 6','click 468 0',
    'expectglobal targetshotcount 1','idle 45','expectglobal towerhitcount 1','expectballs 0','waitpropview gunhand idle 10000')
foreach ($actor in @('bottle1targ','bottle2targ','bottle3targ','can1targ','can2targ')) {
    $actions += @("clickon actor $actor",'waitpropview gunhand idle 10000')
}
$actions += @('expectglobal targetshotcount 6','expectglobal bulletcount 0',
    'clickon prop gunhand','expectpropview gunhand reload')
1..6 | ForEach-Object { $actions += @('click 234 235',"expectglobal bulletcount $_") }
$actions += @('clickon prop gunhand','expectpropview gunhand idle','clickon actor can3targ',
    'expectglobal canhitcount 3','expectglobal bottlehitcount 3','expectglobal targetshotcount 7',
    'waitpropview gunhand idle 10000','clickon button exit','expectset town','expectglobal score fair',
    'idle 60','expectowner matchbox stranger','expectglobal leroyphase 0','expectglobal phase 2',
    'expectglobal playerdeath ','savecheckpoint')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-matchbox.dfsv" -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $actions
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-range-matchbox-native.log"
