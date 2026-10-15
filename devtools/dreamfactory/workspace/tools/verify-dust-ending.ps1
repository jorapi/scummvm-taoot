# SPDX-License-Identifier: GPL-3.0-or-later
# Choose an authored day-five ending, watch credits, and quit through a real click.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day5-morning.dfsv", [ValidateSet('yunni','ranch','mayor','marie','desert')][string]$Ending = 'yunni', [switch]$FullCredits, [switch]$SkipSpeech, [int]$TimeoutSeconds = 900)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$choice = @{ yunni=666; ranch=333; mayor=444; marie=555; desert=777 }[$Ending]
$movie = @{ yunni='yunniend.mov'; ranch='trottend.mov'; mayor='mayorend.mov'; marie='marieend.mov'; desert='deserend.mov' }[$Ending]
$r = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G4 south G14 south)
if ($LASTEXITCODE -or $r.Count -lt 3) { throw 'Cannot plan the finale approach' }
# The last step runs the finale and changes the camera facing during dialogue.
$r = @($r[0..($r.Count-3)])
$checkpoint = "$root/build/save-dust-ending-$Ending-credits.dfsv"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint $checkpoint -TimeoutSeconds $TimeoutSeconds -Steps 1 -SkipSpeech:$SkipSpeech -Choices @(
    'oona.pup:101','jones.pup:222',"oona.pup:$choice"
) -Actions (@('expectglobal day 5','expectglobal clock 1','expectglobal phase 0','expectowner chest stranger') + $r + @(
    'expectglobal phase 99','expectpropview names 1','dumpprop names','expectglobal playerdeath ','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-ending-$Ending-native.log"
if ((Get-Content "$root/build/dust-ending-$Ending-native.log" -Raw) -notmatch ('MOVIE PLAY '+[regex]::Escape($movie))) { throw "Missing ending movie: $movie" }
$a = @('expectglobal day 5','expectglobal phase 99','expectpropview names 1')
if ($FullCredits) {
    2..8 | ForEach-Object { $a += "waitpropview names $_ 60000" }
    $a += 'waitpropview names 1 60000'
} else { $a += 'idle 80' }
$a += @('clickquit 256 200','expectglobal phase 99')
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $checkpoint -TimeoutSeconds $TimeoutSeconds -Steps 1 -Actions $a
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-ending-$Ending-credits-native.log"
if ((Get-Content "$root/build/dust-ending-$Ending-credits-native.log" -Raw) -notmatch 'BOOT SCRIPT QUIT PASS') { throw 'Credits did not quit through the authored handler' }
Write-Output "Dust $Ending ending and scripted credits quit PASS"
