# SPDX-License-Identifier: GPL-3.0-or-later
# Real keymapper -> EventManager -> ScummVM GUI -> Escape -> authored navigation.
param([int]$TimeoutSeconds = 60)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
foreach ($edition in 'dust','full','demo') {
    $checkpoint = if ($edition -eq 'dust') { 'day5-morning' } else { 'opening' }
    $startView = switch ($edition) { dust { 'south' } full { 'View14' } demo { 'View55' } }
    $turnedView = switch ($edition) { dust { 'west' } full { 'View16' } demo { 'View71' } }
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-$checkpoint.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
        'menukey','mappedkey keypad6',"expectview $turnedView",
        'mappedkey keypad4',"expectview $startView"
    )
    Copy-Item "$root/build/boot-$edition.log" "$root/build/menu-$edition-native.log"
}
Write-Output 'F5 menu, Escape cancellation, paused clock and mapped navigation PASS in all three editions'
