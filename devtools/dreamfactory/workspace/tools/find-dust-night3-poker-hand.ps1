# SPDX-License-Identifier: GPL-3.0-or-later
# Find a replayable natural win by ordinary waiting before accepting the offer.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-night3-next2-result.dfsv", [int[]]$WaitFrames = @(0,10,20,40,60,80,100,120), [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$route = @(& node "$PSScriptRoot/dust-route.mjs" SALLOWER C4 west C2 east)
if ($LASTEXITCODE) { throw 'Cannot plan poker table approach' }
for ($trial = 0; $trial -lt $WaitFrames.Count; $trial++) {
    $checkpoint = "$root/build/save-dust-night3-poker-trial$trial.dfsv"
    & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint $checkpoint -SkipSpeech -TimeoutSeconds $TimeoutSeconds -Choices @('mez.pup:101','mez.pup:101') -Actions ($route + @(
        'expectglobal day 3','expectglobal clock 3','expectglobal phase 1',"idle $($WaitFrames[$trial])",
        'clickon prop gamblers','expectflat flat 0','expectglobal playerphase 1','savecheckpoint'
    )) *> "$root/build/dust-night3-poker-trial$trial-run.log"
    Copy-Item "$root/build/boot-dust.log" "$root/build/dust-night3-poker-trial$trial-native.log"
    $json = & python "$PSScriptRoot/dust-poker-plan.py" $checkpoint
    if ($LASTEXITCODE) { throw 'Cannot inspect the earned hand' }
    $plan = $json | ConvertFrom-Json
    Write-Output "Trial $trial wait=$($WaitFrames[$trial]) best=$($plan.score) opponents=$($plan.opponents | ConvertTo-Json -Compress)"
    if ($plan.winsWithoutFolds) {
        Copy-Item $checkpoint "$root/build/save-dust-night3-poker-natural-ready.dfsv"
        $json | Set-Content "$root/build/dust-night3-poker-natural-plan.json"
        Write-Output "Natural hand ready PASS: discard $($plan.discards -join ', ')"
        return
    }
}
throw 'No natural winning draw found in these ordinary waits'
