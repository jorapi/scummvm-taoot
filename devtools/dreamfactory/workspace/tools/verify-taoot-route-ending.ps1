# SPDX-License-Identifier: GPL-3.0-or-later
param([string]$RestoreCheckpoint = "build/save-full-best-sinking-phase1.dfsv")
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
 $run=Join-Path $root ('build/route-ending-full/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
 New-Item -ItemType Directory -Path $run -Force | Out-Null
 $route=@('expectview View250','key rightarrow','expectview View256','key uparrow','expectview View211')
 & tools/trace-boot.ps1 -Editions full -RestoreCheckpoint $RestoreCheckpoint -SavePath "$run/saves" -Steps 1 -SkipSpeech -SkipMovies @('leave.mov','debris.mov','prozac.mov') -TimeoutSeconds 900 -Choices @('jones2.pup:101') -MovieClicks @((1..14 | ForEach-Object {'credits.mov:#1'})) -Actions ($route + @('expectview View211','keyquit uparrow','expectglobal mission good','dumproutes')) *> "$run/suite.log"
 Copy-Item -LiteralPath build/boot-full.log -Destination "$run/native.log"
 $log=Get-Content "$run/native.log" -Raw
 if($log -notmatch 'ROUTE ENDING node=\d+ movie=prozac.mov outcome=A changed future' -or $log -notmatch 'BOOT ROUTE node=\d+ kind=2 .*speaker=A changed future' -or $log -notmatch 'BOOT SCRIPT QUIT PASS') {throw 'Best ending route evidence missing'}
 @{scope='Earned Titanic best-ending checkpoint; physical navigation and farewell choice, authored narration, movie skips and credits clicks; persisted outcome and scripted quit'; passed=$true; binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content "$run/proof.json"
 Write-Output "Titanic authored best ending captured in route graph PASS: $run"
} finally {Pop-Location}
