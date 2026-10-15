# SPDX-License-Identifier: GPL-3.0-or-later
# Advance through ordinary town navigation after obtaining two equipment items.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-boots.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Run-Trace([string]$Name,[string]$Restore,[string[]]$Actions) {
    $checkpoint = "$root/build/save-dust-$Name.dfsv"
    & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $Restore -Checkpoint $checkpoint -TimeoutSeconds $TimeoutSeconds -Actions $Actions | Set-Content "$root/build/dust-$Name-trace.log"
    Copy-Item "$root/build/boot-dust.log" "$root/build/dust-$Name-native.log"
    return $checkpoint
}
# Keep detailed trace rows in per-stage suite logs.
$outside = "$root/build/save-dust-morning-outside.dfsv"
Run-Trace morning-outside $RestoreCheckpoint @(
    'expectglobal day 2','expectglobal clock 1','expectowner gun stranger','expectowner boots stranger',
    'key leftarrow','expectview west','click 250 150','expectowner door underout',
    'key uparrow','expectset town','expectscene Scene A7','expectview north','savecheckpoint'
) | Write-Output
$current = $outside
for ($step = 1; $step -le 24; $step++) {
    $state = & python "$PSScriptRoot/read-checkpoint-globals.py" $current day clock
    if ($LASTEXITCODE) { throw 'Cannot read checkpoint clock' }
    $state = $state | ConvertFrom-Json
    if ($state.day -ne 2) { throw 'Unexpected story day' }
    if ($state.clock -eq 2) { break }
    if ($state.clock -ne 1) { throw 'Unexpected story clock' }
    $log = Get-Content "$root/build/boot-dust.log" -Raw
    $location = [regex]::Match($log, 'BOOT LOCATION room=town scene=(.*?) view=(.*?) stage=')
    if (!$location.Success) { throw 'Missing native town standpoint' }
    $route = & node "$PSScriptRoot/dust-route.mjs" TOWN $location.Groups[1].Value $location.Groups[2].Value G9 east
    if ($LASTEXITCODE) { throw 'Cannot plan town movement' }
    $keys = @($route | Where-Object { $_.StartsWith('key ') })
    $key = if ($keys.Count) { $keys[0] } else { 'key leftarrow' }
    $name = "afternoon-walk-$step"
    Run-Trace $name $current @($key,'idle 40','expectglobal playerdeath ','savecheckpoint') | Write-Output
    $current = "$root/build/save-dust-$name.dfsv"
}
$final = "$root/build/save-dust-day2-afternoon.dfsv"
Run-Trace day2-afternoon $current @(
    'expectglobal day 2','expectglobal clock 2','expectglobal phase 0','expectset town',
    'expectowner gun stranger','expectowner boots stranger','expectowner sugarcubes stranger',
    'expectglobal playerdeath ','savecheckpoint','targets'
) | Write-Output
Write-Output "Dust day-two afternoon PASS: $final"
