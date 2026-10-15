# SPDX-License-Identifier: GPL-3.0-or-later
param([string]$RestoreCheckpoint = "build/save-demo-photograph.dfsv")
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
 $run=Join-Path $root ('build/route-ending-demo/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
 New-Item -ItemType Directory -Path $run -Force | Out-Null
 function Plan($room,$from,$to) {
  $route=@(& node tools/room-route.mjs demo $room $from $to | ForEach-Object { ($_ -split ' # ')[0] })
  if($LASTEXITCODE){throw 'Room route planning failed'}
  return $route
 }
 $actions=@('expectflat main 1','expectview View33','expectowner photo1 frank') + @(Plan B59 View33 View18) + @(
 'expectview View18','clickon painting halla','key uparrow','expectset halla','expectview View44'
 ) + @(Plan HALLA View44 View25) + @(
 'expectview View25','clickon painting gstair2,decka','key uparrow','expectset gstair2','expectview View27'
 ) + @(Plan GSTAIR2 View27 View73) + @(
 'expectset gstair1','expectview View27'
 ) + @(Plan GSTAIR1 View27 View38) + @(
 'expectscene Scene18','expectview View38','clickon actor penny','expectglobal phase 2','expectflat demo 1','expectset none','dumproutes'
 )
 & tools/trace-boot.ps1 -Editions demo -RestoreCheckpoint $RestoreCheckpoint -SavePath "$run/saves" -Steps 1 -SkipSpeech -SkipMovies @('postdemo.mov') -TimeoutSeconds 300 -Choices @('dpenny.pup:101','dpenny.pup:101') -Actions $actions *> "$run/suite.log"
 Copy-Item -LiteralPath build/boot-demo.log -Destination "$run/native.log"
 $log=Get-Content "$run/native.log" -Raw
 if($log -notmatch 'ROUTE ENDING node=\d+ movie=postdemo.mov outcome=Interactive demo conclusion' -or $log -notmatch 'BOOT ROUTE node=\d+ kind=2 .*speaker=Interactive demo conclusion') {throw 'Demo conclusion route evidence missing'}
 @{scope='Earned developed-photograph checkpoint; player navigation and Penny dialogue, demo conclusion movie skip and persisted route outcome'; passed=$true; binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content "$run/proof.json"
 Write-Output "Supplied demo authored conclusion captured in route graph PASS: $run"
} finally {Pop-Location}
