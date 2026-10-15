# SPDX-License-Identifier: GPL-3.0-or-later
# Satisfy the authored afternoon gate, then advance using normal town movement.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-mask.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Run-Trace([string]$Name,[string]$Restore,[string[]]$Actions) {
    $checkpoint = "$root/build/save-dust-$Name.dfsv"
    & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $Restore -Checkpoint $checkpoint -TimeoutSeconds $TimeoutSeconds -Actions $Actions | Set-Content "$root/build/dust-$Name-trace.log"
    Copy-Item "$root/build/boot-dust.log" "$root/build/dust-$Name-native.log"
    return $checkpoint
}
$dining = @(& node "$PSScriptRoot/dust-route.mjs" MAYDINE E3 east D2 west)
if ($LASTEXITCODE) { throw 'Cannot plan dining-room exit' }
$hall = @(& node "$PSScriptRoot/dust-route.mjs" MAYHALL C3 west C4 south)
if ($LASTEXITCODE) { throw 'Cannot plan mansion exit' }
$current = Run-Trace day3-afternoon-ready $RestoreCheckpoint (@(
    'expectglobal day 3','expectglobal clock 2','expectowner mask stranger',
    'expectowner yunnibook stranger','expectowner flute stranger'
) + $dining + @('click 250 150','expectowner door hall2','key uparrow','expectset mayhall') + $hall + @(
    'click 250 150','expectowner door front','key uparrow','expectset town',
    'expectglobal playerdeath ','savecheckpoint'
))
for ($step = 1; $step -le 32; $step++) {
    $state = & python "$PSScriptRoot/read-checkpoint-globals.py" $current day clock
    if ($LASTEXITCODE) { throw 'Cannot read checkpoint clock' }
    $state = $state | ConvertFrom-Json
    if ($state.day -ne 3) { throw 'Unexpected story day' }
    if ($state.clock -eq 3) { break }
    if ($state.clock -ne 2) { throw 'Unexpected story clock' }
    $log = Get-Content "$root/build/boot-dust.log" -Raw
    $location = [regex]::Match($log, 'BOOT LOCATION room=town scene=(.*?) view=(.*?) stage=')
    if (!$location.Success) { throw 'Missing native town standpoint' }
    $route = & node "$PSScriptRoot/dust-route.mjs" TOWN $location.Groups[1].Value $location.Groups[2].Value G9 east
    if ($LASTEXITCODE) { throw 'Cannot plan town movement' }
    $keys = @($route | Where-Object { $_.StartsWith('key ') })
    $key = if ($keys.Count) { $keys[0] } else { 'key leftarrow' }
    $current = Run-Trace "day3-night-walk-$step" $current @($key,'idle 40','expectglobal playerdeath ','savecheckpoint')
}
$final = Run-Trace day3-night $current @(
    'expectglobal day 3','expectglobal clock 3','expectglobal phase 0','expectset town',
    'expectowner mask stranger','expectowner yunnibook stranger','expectowner flute stranger',
    'expectowner boots stranger','expectowner bullets stranger','expectglobal playerdeath ','savecheckpoint','targets'
)
Write-Output "Dust day-three night PASS: $final"
