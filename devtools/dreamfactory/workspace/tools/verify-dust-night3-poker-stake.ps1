# SPDX-License-Identifier: GPL-3.0-or-later
# Earn the stake through normal blackjack bets, preserving day/night/phase.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-night3-reconciled.dfsv", [int]$TimeoutSeconds = 240)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$town = @(& node "$PSScriptRoot/dust-route.mjs" NITE B11 west G8 west)
if ($LASTEXITCODE) { throw 'Cannot plan saloon approach' }
$saloon = @(& node "$PSScriptRoot/dust-route.mjs" SALLOWER D1 west C4 west)
if ($LASTEXITCODE) { throw 'Cannot plan blackjack approach' }
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-dust-night3-stake-entry.dfsv" -Steps 1 -TimeoutSeconds $TimeoutSeconds -Actions ($town + @(
    'click 270 100','expectowner door saloon','key uparrow','expectset sallower'
) + $saloon + @('expectglobal phase 1','expectglobal playercash 4','savecheckpoint'))
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night3-stake-entry-native.log"
& "$PSScriptRoot/verify-dust-blackjack-cash.ps1" -RestoreCheckpoint "$root/build/save-dust-night3-stake-entry.dfsv" -MinimumCash 21 -MaximumHands 11 -ResultPrefix night3-stake-route -SkipSpeech -PlayAllHands -BetSequence @(4,1,1,1,1,4,1,7,10,1,1) -TimeoutSeconds $TimeoutSeconds
$source = "$root/build/save-dust-night3-stake-route-result.dfsv"
$json = & python "$PSScriptRoot/read-checkpoint-globals.py" $source day clock phase playercash
if ($LASTEXITCODE) { throw 'Cannot inspect earned stake' }
$state = $json | ConvertFrom-Json
if ($state.day -ne 3 -or $state.clock -ne 3 -or $state.phase -ne 1 -or $state.playercash -lt 21) { throw 'Earned stake or story phase differs' }
Copy-Item $source "$root/build/save-dust-night3-poker-funded.dfsv"
Write-Output "Dust third-night poker stake earned PASS: $($state.playercash)"
