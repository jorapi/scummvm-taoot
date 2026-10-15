# SPDX-License-Identifier: GPL-3.0-or-later
# Return to the hotel and pass Bloodstone's automatic first-night parting speech.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-jones-warning.dfsv", [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/dust-route.mjs" HOTLOWER A1 east A2 south
if ($LASTEXITCODE) { throw 'Cannot plan hotel approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-night-wine.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions (@(
    'expectglobal phase 8','expectset town','expectscene Scene G5','expectview south',
    'key leftarrow','expectview east','click 250 150','key uparrow','expectset hotlower'
) + @($route | Where-Object { $_ -ne 'expectview south' }) + @(
    'expectglobal phase 9','expectview east','expectowner hrkey stranger',
    'expectowner cigar stranger','expectglobal playerdeath ','savecheckpoint'
))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night-wine-native.log"
