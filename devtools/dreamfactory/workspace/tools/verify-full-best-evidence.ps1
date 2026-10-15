# SPDX-License-Identifier: GPL-3.0-or-later
# Replay the retail evidence route with the completed necklace quest retained.
param([ValidateSet('prepare','necklace','mission2','mission3','sinking','all')][string]$Stage = 'necklace', [int]$TimeoutSeconds = 900, [string]$StartAt = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$routes = @(
    @{ Stage='prepare'; Name='fencing'; Script='verify-full-fencing.ps1'; Input='painting'; Output='ring'; Suffix='pre-necklace-ring' },
    @{ Stage='necklace'; Name='georgia'; Script='verify-full-necklace.ps1'; Input='best-pre-necklace-ring'; Output='georgia-necklace'; MissionPhase=2 },
    @{ Stage='necklace'; Name='cabin'; BoatDeckJones=$true; Script='verify-full-georgia-cabin.ps1'; Input='best-georgia-necklace'; Output='georgia-b70' },
    @{ Stage='necklace'; Name='charles'; Script='verify-full-charles-necklace.ps1'; Input='best-georgia-b70'; Output='charles-necklace' },
    @{ Stage='necklace'; Name='fuses'; Script='verify-full-necklace-fuses.ps1'; Input='best-charles-necklace'; Output='sasha-poweron' },
    @{ Stage='necklace'; Name='doll'; Script='verify-full-necklace-doll.ps1'; Input='best-sasha-poweron'; Output='necklace-swapped'; Necklace=$true },
    @{ Stage='necklace'; Name='escape'; Script='verify-full-necklace-escape.ps1'; Input='best-necklace-swapped'; Output='necklace-recovered'; MissionPhase=2; Necklace=$true },
    @{ Stage='necklace'; Name='ring-room'; Script='verify-full-best-ring-resume.ps1'; Input='best-necklace-recovered'; Output='necklace-ring-ready'; Necklace=$true },
    @{ Stage='mission2'; Name='mission2'; Script='verify-full-mission2.ps1'; Input='best-necklace-ring-ready'; Output='mission3'; Necklace=$true },
    @{ Stage='mission3'; Name='bath'; Script='verify-full-bath-investigation.ps1'; Input='best-mission3'; Output='rubiclue' },
    @{ Stage='mission3'; Name='hacker'; Script='verify-full-hacker-clue.ps1'; Input='best-rubiclue'; Output='hacker-clue' },
    @{ Stage='mission3'; Name='cigarettes'; Script='verify-full-cigarette-lead.ps1'; Input='best-hacker-clue'; Output='smokestack-lead' },
    @{ Stage='mission3'; Name='turbine'; Script='verify-full-turbine2.ps1'; Input='best-smokestack-lead'; Output='turbine2' },
    @{ Stage='mission3'; Name='fight'; Script='verify-full-vlad-fight.ps1'; Input='best-turbine2'; Output='vlad-defeated' },
    @{ Stage='mission3'; Name='notebook'; Script='verify-full-notebook.ps1'; Input='best-vlad-defeated'; Output='notebook-recovered' },
    @{ Stage='mission3'; Name='transition'; Script='verify-full-mission3.ps1'; Input='best-notebook-recovered'; Output='mission4' },
    @{ Stage='sinking'; Name='shawl'; Script='verify-full-sinking-shawl.ps1'; Input='best-mission4'; Output='sinking-shawl' },
    @{ Stage='sinking'; Name='trade'; Script='verify-full-sinking-notebook.ps1'; Input='best-sinking-shawl'; Output='sinking-notebook' },
    @{ Stage='sinking'; Name='clock'; Script='verify-full-sinking-clock.ps1'; Input='best-sinking-notebook'; Output='sinking-clock' },
    @{ Stage='sinking'; Name='boatdeck'; Script='verify-full-sinking-return.ps1'; Input='best-sinking-clock'; Output='sinking-phase1' }
)
if ($StartAt -and !@($routes | Where-Object { $_.Name -eq $StartAt -and ($Stage -eq 'all' -or $_.Stage -eq $Stage) }).Count) { throw "Unknown starting route: $StartAt" }
$started = !$StartAt
foreach ($route in $routes) {
    if ($Stage -ne 'all' -and $route.Stage -ne $Stage) { continue }
    if (!$started) { if ($route.Name -eq $StartAt) { $started = $true } else { continue } }
    $source = "$root/build/save-full-$($route.Input).dfsv"
    if (!(Test-Path -LiteralPath $source)) { throw "Missing route checkpoint: $source" }
    Write-Output "Best evidence starting: $($route.Name)"
    $options = @{ RestoreCheckpoint=$source; TimeoutSeconds=$TimeoutSeconds }
    if ($route.BoatDeckJones) { $options.BoatDeckJones = $true }
    if ($route.ContainsKey('MissionPhase')) { $options.MissionPhase = $route.MissionPhase }
    try {
        & "$PSScriptRoot/$($route.Script)" @options *> "$root/build/full-best-$($route.Name)-suite.log"
    } catch {
        Copy-Item "$root/build/boot-full.log" "$root/build/full-best-$($route.Name)-failed-native.log"
        throw
    }
    $suffix = if ($route.Suffix) { $route.Suffix } else { $route.Output }
    $checkpoint = "$root/build/save-full-best-$suffix.dfsv"
    Copy-Item "$root/build/save-full-$($route.Output).dfsv" $checkpoint
    $checks = @('expectowner painting frank','expectowner rubaiyat frank','expectglobal playerdeath ')
    if ($route.Necklace -or $route.Stage -in @('mission3','sinking')) { $checks += @('expectowner realneck frank','expectowner fakeneck vlad') }
    & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $checkpoint -Steps 1 -TimeoutSeconds 60 -Actions $checks *> "$root/build/full-best-$($route.Name)-evidence.log"
    Copy-Item "$root/build/boot-full.log" "$root/build/full-best-$($route.Name)-evidence-native.log"
    Write-Output "Best evidence PASS: $($route.Name)"
}
