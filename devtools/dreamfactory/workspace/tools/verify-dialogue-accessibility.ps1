# SPDX-License-Identifier: GPL-3.0-or-later
param([switch]$CollectOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/dialogue-accessibility/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    $binary = Get-Item build/scummvm/Release/scummvm.exe
    if (!$CollectOnly) {
        & tools/verify-scripts.ps1 -Puppets *> "$run/puppets.log"
        & tools/verify-scripts.ps1 -Stages *> "$run/stages.log"
    }
    $counts = @{}
    foreach ($edition in @('full','demo','dust')) {
        foreach ($kind in @('puppet','stage')) {
            $log = Get-Item "build/$kind-$edition.log"
            if ($log.LastWriteTimeUtc -lt $binary.LastWriteTimeUtc) {throw "$edition $kind evidence predates the current executable"}
            Copy-Item $log.FullName "$run/$edition-$kind.log"
        }
        $speech = Get-Content "$run/$edition-puppet.log" -Raw
        foreach ($marker in @('CAPTION STYLE PASS','CAPTION CORPUS PASS','CAPTION AUTHORED PASS','SHARED AUDIO PASS','DIALOGUE OPTIONS PASS.*desktop','DIALOGUE OPTIONS PASS.*480x272','HEARD REPLAY PASS')) {
            if ($speech -notmatch $marker) {throw "$edition lacks accessibility evidence: $marker"}
        }
        $counts[$edition] = [int][regex]::Match($speech, 'CAPTION CORPUS PASS authored=(\d+)').Groups[1].Value
        $state = Get-Content "$run/$edition-stage.log" -Raw
        foreach ($marker in @('JOURNAL SAVE PASS','ROUTE INVENTORY PASS','ROUTE SAVE PASS')) {if ($state -notmatch $marker) {throw "$edition lacks state compatibility evidence: $marker"}}
        foreach ($capture in @('caption','reply','repeat')) {
            Copy-Item "build/puppet-session-$edition/accessibility-$capture-preview.rgba" "$run/$edition-$capture.rgba"
            & python tools/preview-rgba.py "$run/$edition-$capture.rgba"
            if ($LASTEXITCODE) {throw 'Native caption capture export failed'}
        }
        foreach ($size in @('desktop','handheld')) {
            foreach ($action in @('save','cancel')) {Copy-Item "build/puppet-session-$edition/accessibility-options-$size-$action.bmp" "$run/$edition-options-$size-$action.bmp"}
        }
    }
    @{passed=$true; binarySha256=(Get-FileHash $binary.FullName -Algorithm SHA256).Hash; authoredCaptionRecords=$counts; collectedExistingResults=[bool]$CollectOnly; scope='Native shared Edit Game settings save/cancel and legacy migration at desktop/480x272; caption font 20, all supplied Windows PUP text-record bounds, opacity 0/50/100, friendly speaker aliases and neutral fallback, long controller reply preview, speech replay, shared music/effects/speech settings and mute, native state/route compatibility. Original portrait captures included. Physical handheld runtime remains untested.'} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Dialogue accessibility native proof PASS: $run"
} finally {Pop-Location}
