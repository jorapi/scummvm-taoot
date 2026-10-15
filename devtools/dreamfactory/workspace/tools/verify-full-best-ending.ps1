# SPDX-License-Identifier: GPL-3.0-or-later
# Complete the retail best ending, including unskipped credits and scripted quit.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-best-sinking-phase1.dfsv", [int]$TimeoutSeconds = 1500)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$PSScriptRoot/room-route.mjs" full DECKBD2 View250 View211 1
if ($LASTEXITCODE) { throw 'Cannot plan the Gorse-Jones route' }
$route = @($route | ForEach-Object { ($_ -split ' # ')[0] })
& "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('jones2.pup:101') -MovieClicks @((1..14 | ForEach-Object { 'credits.mov:#1' })) -Actions (@(
    'expectset deckbd2', 'expectview View250', 'expectglobal mission 4', 'expectglobal phase 1',
    'expectowner realneck frank', 'expectowner notebook frank', 'expectowner painting frank', 'expectowner rubaiyat frank'
) + $route + @('expectview View211', 'keyquit uparrow', 'expectglobal mission good'))
Copy-Item "$root/build/boot-full.log" "$root/build/full-best-ending-native.log"
$endingLog = Get-Content "$root/build/full-best-ending-native.log" -Raw
foreach ($movie in @('leave.mov','debris.mov','prozac.mov','credits.mov')) {
    if ($endingLog -notmatch ('MOVIE PLAY ' + [regex]::Escape($movie))) { throw "Missing best-ending movie: $movie" }
}
if ([regex]::Matches($endingLog, 'BOOT MOVIE CLICK file=credits\.mov region=#1').Count -ne 14) { throw 'Not all credits pages were advanced through physical clicks' }
if ($endingLog -notmatch 'BOOT SCRIPT QUIT PASS') { throw 'Best ending did not quit through its authored opcode' }
if ($endingLog -match 'MOVIE PLAY (playmore|bktoship)\.mov') { throw 'Best ending unexpectedly took the unsuccessful-ending return path' }
Write-Output 'Retail best ending, credits and scripted quit PASS'
