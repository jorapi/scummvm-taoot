# SPDX-License-Identifier: GPL-3.0-or-later
# Re-enter normally to obtain random layouts; never set the maze global or RNG.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-vlad-defeated.dfsv", [ValidateRange(1,4)][int[]]$Layouts = @(2,4), [int]$MaximumEntries = 24, [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$routeOut = & node "$PSScriptRoot/room-route.mjs" full SMSTACK1 View40 View41
if ($LASTEXITCODE) { throw 'Cannot plan return from the smokestack entrance' }
$routeOut = @($routeOut | ForEach-Object { ($_ -split ' # ')[0] })
$remaining = @($Layouts | Select-Object -Unique)
$current = $RestoreCheckpoint
for ($entry = 1; $entry -le $MaximumEntries -and $remaining.Count; ++$entry) {
    $actions = @('expectglobal mission 3','expectglobal phase 2')
    if ($entry -gt 1) { $actions += $routeOut + @('key uparrow','expectset engine','expectview View119') }
    $actions += @('key leftarrow','key leftarrow','expectview View120','key uparrow',
        'expectset smstack1','expectview View40','expectglobal stacklevel 1','savecheckpoint')
    $next = "$root/build/save-full-smokestack-draw$entry.dfsv"
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $current -Checkpoint $next -TimeoutSeconds $TimeoutSeconds -SkipMovies @('stackin.mov','stackout.mov') -Actions $actions
    Copy-Item "$root/build/boot-full.log" "$root/build/full-smokestack-draw$entry-native.log"
    $state = & python "$PSScriptRoot/read-checkpoint-globals.py" $next mazenumber | ConvertFrom-Json
    if ($LASTEXITCODE -or [int]$state.mazenumber -notin 1..4) { throw 'Invalid authored maze number' }
    $maze = [int]$state.mazenumber
    if ($maze -in $remaining) {
        $route = & node "$PSScriptRoot/smokestack-route.mjs" $maze
        if ($LASTEXITCODE) { throw "Cannot plan maze $maze" }
        & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $next -Checkpoint "$root/build/save-full-smokestack-maze$maze-top.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions (@(
            "expectglobal mazenumber $maze",'expectglobal stacklevel 1'
        ) + @($route) + @('expectset smstack3','expectview View53','expectowner notebook none','savecheckpoint'))
        Copy-Item "$root/build/boot-full.log" "$root/build/full-smokestack-maze$maze-native.log"
        & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-smokestack-maze$maze-top.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
            "expectglobal mazenumber $maze",'expectset smstack3','expectview View53','expectowner notebook none',
            'key rightarrow','key leftarrow','expectview View53'
        )
        Copy-Item "$root/build/boot-full.log" "$root/build/full-smokestack-maze$maze-restore-native.log"
        $remaining = @($remaining | Where-Object { $_ -ne $maze })
    }
    $current = $next
}
if ($remaining.Count) { throw "Layouts not naturally drawn within $MaximumEntries entries: $($remaining -join ', ')" }
Write-Output "Titanic smokestack layouts $($Layouts -join ', ') traversal and fresh top-floor restore PASS"
