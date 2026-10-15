# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/dialogue-replay/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    & tools/verify-scripts.ps1 -Puppets *> "$run/puppets.log"
    & tools/verify-scripts.ps1 -Stages *> "$run/stages.log"
    foreach ($edition in @('full','demo','dust')) {
        Copy-Item "build/puppet-$edition.log" "$run/$edition-puppets.log"
        Copy-Item "build/stage-$edition.log" "$run/$edition-stages.log"
        $speech = Get-Content "$run/$edition-puppets.log" -Raw
        foreach ($marker in @('HEARD REPLAY PASS','HEARD AUTHORED PASS','DIALOGUE LINE SKIP HOST PASS','GAMEPAD X LINE SKIP PASS')) {
            if ($speech -notmatch $marker) {throw "$edition lacks native replay evidence: $marker"}
        }
        $state = Get-Content "$run/$edition-stages.log" -Raw
        foreach ($marker in @('HEARD STORE PASS','HEARD SAVE PASS','ROUTE INVENTORY PASS')) {
            if ($state -notmatch $marker) {throw "$edition lacks native history/save evidence: $marker"}
        }
        Copy-Item "build/puppet-session-$edition/replay-preview.rgba" "$run/$edition.rgba"
        & python tools/preview-rgba.py "$run/$edition.rgba"
        if ($LASTEXITCODE) {throw 'Native replay screenshot export failed'}
    }
    & tools/verify-heard-history.ps1 *> "$run/real-history.log"
    @{passed=$true; binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash; scope='Native mapped F10/right-stick, optional touch/cursor-A control, refused unheard activation, mixed known/new sequence, choice stop, silent known playback, original queue/pose and separate line/full skips. Original authored portrait/voice playback and screenshots for all three editions; durable history across a real Dust pre-trade restore. Handheld physical runtime remains untested.'} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Dialogue replay native proof PASS: $run"
} finally {Pop-Location}
