# SPDX-License-Identifier: GPL-3.0-or-later
# Verify naturally armed/unarmed timers through physical navigation and saves.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-turbine2.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Route([string]$Room, [string]$From, [string]$To) {
    $actions = & node "$PSScriptRoot/room-route.mjs" full $Room $From $To
    if ($LASTEXITCODE) { throw "Cannot plan $Room route" }
    @($actions | ForEach-Object { ($_ -split ' # ')[0] })
}
function Trace-Timer([string]$Name, [string]$Source, [string]$Destination, [string[]]$Actions) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $Source -Checkpoint $Destination -TimeoutSeconds $TimeoutSeconds -Actions $Actions
    Copy-Item "$root/build/boot-full.log" "$root/build/full-ambient-$Name-native.log"
}
function Observations([string]$Name) {
    $text = Get-Content "$root/build/full-ambient-$Name-native.log" -Raw
    @([regex]::Matches($text, 'BOOT AMBIENT name=steam1[^\r\n]*count=(\d+) armed=(\d+)') | ForEach-Object {
        [pscustomobject]@{ Count = [int]$_.Groups[1].Value; Armed = [int]$_.Groups[2].Value }
    })
}
$armed = "$root/build/save-full-ambient-armed.dfsv"
$unarmed = "$root/build/save-full-ambient-unarmed.dfsv"
Trace-Timer 'armed' $RestoreCheckpoint $armed (@('expectset control','expectview View24') + @(Route CONTROL View24 View14) + @(
    'clickon painting door','key uparrow','expectset turb','expectview View75',
    'idle 3','dumpcrickets','savecheckpoint'
))
$initial = @(Observations 'armed')[-1]
if ($initial.Armed -ne 1 -or $initial.Count -le 0 -or $initial.Count -gt 400) { throw 'Steam timer did not arm naturally' }
Trace-Timer 'armed-restore' $armed $unarmed (@(
    "expectcricket steam1 1 $($initial.Count)",'idle 3','dumpcrickets'
) + @(Route TURB View75 View17) + @('dumpcrickets','savecheckpoint'))
$loaded = @(Observations 'armed-restore')
if ($loaded.Count -ne 3 -or $loaded[1].Count -ge $initial.Count -or $loaded[-1].Armed -ne 0) { throw 'Loaded timer did not advance and disarm' }
$frozen = $loaded[-1].Count
Trace-Timer 'unarmed-restore' $unarmed "$root/build/save-full-ambient-rearmed.dfsv" (@(
    "expectcricket steam1 0 $frozen",'idle 3',"expectcricket steam1 0 $frozen"
) + @(Route TURB View17 View75) + @('idle 3','dumpcrickets','savecheckpoint'))
$returned = @(Observations 'unarmed-restore')[-1]
if ($returned.Armed -ne 1 -or $returned.Count -le 0 -or $returned.Count -gt 400) { throw 'Returning to range did not rearm' }
$invalid = "$root/build/save-full-invalid-cricket-armed.dfsv"
& python "$PSScriptRoot/cricket-save-fixtures.py" $unarmed $invalid
if ($LASTEXITCODE) { throw 'Invalid armed-state fixture generation failed' }
Trace-Timer 'armed-rejection' $unarmed $invalid @(
    "expectcricket steam1 0 $frozen",'rejectcheckpoint',"expectcricket steam1 0 $frozen",'expectview View17'
)
Write-Output 'Titanic naturally armed timer, exact fresh countdown, frozen unarmed restore, range reentry and invalid flag rejection PASS'
