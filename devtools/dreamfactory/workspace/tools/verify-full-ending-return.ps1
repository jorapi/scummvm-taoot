# SPDX-License-Identifier: GPL-3.0-or-later
# Leave before recovering Vlad's notebook; exercise the authored failed ending.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-full-sinking-boatdeck.dfsv", [int]$TimeoutSeconds = 1200)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = & node "$root/tools/room-route.mjs" full DECKBD2 View114 View211 1
if ($LASTEXITCODE) { throw 'Cannot plan the Gorse-Jones route' }
$route = @($route | ForEach-Object { ($_ -split ' # ')[0] })
# Walking forward from the lifeboat standpoint opens the authored encounter.
# A real missing-evidence precondition prevents a best-route save from silently
# taking the credits path instead of exercising the return-to-ship branch.
& "$root/tools/trace-boot.ps1" -Editions full -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-full-retail-ending-return.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices @('jones2.pup:101') -MovieClicks @('playmore.mov:#1') -Actions (@(
    'expectset deckbd2', 'expectview View114', 'expectglobal mission 4', 'expectglobal phase 0',
    'expectowner notebook vlad', 'expectowner painting frank', 'expectowner rubaiyat frank', 'expectowner realneck frank'
) + $route + @(
    'key uparrow', 'expectglobal mission 1', 'expectglobal phase 0', 'expectset c73', 'expectview View63', 'savecheckpoint'
))
Copy-Item "$root/build/boot-full.log" "$root/build/full-retail-ending-return-native.log"
$endingLog = Get-Content "$root/build/full-retail-ending-return-native.log" -Raw
foreach ($movie in @('leave.mov', 'debris.mov', 'rushend.mov', 'playmore.mov', 'bktoship.mov')) {
    if ($endingLog -notmatch ('MOVIE PLAY ' + [regex]::Escape($movie))) { throw "Missing ending movie: $movie" }
}
if ($endingLog -notmatch 'SCRIPT MESSAGE notebook:vlad\s+painting:frank\s+rubaiyat:frank\s+real necklace:frank') { throw 'Narration did not observe the earned missing-notebook case' }
if ($endingLog -match 'MOVIE PLAY (prozac|credits)\.mov') { throw 'Failed-ending test unexpectedly took the best ending' }
Write-Output 'Retail missing-notebook ending and return-to-ship PASS'
& "$root/tools/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-retail-ending-return.dfsv" -Steps 1 -TimeoutSeconds 60 -Actions @(
    'expectglobal mission 1', 'expectglobal phase 0', 'expectset c73', 'expectview View63',
    'key rightarrow', 'expectview View65'
)
Copy-Item "$root/build/boot-full.log" "$root/build/full-retail-ending-restore-native.log"
Write-Output 'Retail ending reset checkpoint restoration PASS'
