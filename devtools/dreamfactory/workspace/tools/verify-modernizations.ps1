# SPDX-License-Identifier: GPL-3.0-or-later
param([switch]$CollectOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/modernizations/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Force $run | Out-Null
    if (!$CollectOnly) {
        & tools/verify-scripts.ps1 *> "$run/scripts.log"
        & tools/verify-scripts.ps1 -Stages *> "$run/stages.log"
        & tools/verify-scripts.ps1 -Puppets *> "$run/puppets.log"
    }
    function Verify([string]$name, [scriptblock]$test) {
        & $test *> "$run/$name.log"
        if (!$?) {throw "$name verification failed"}
        $result = Get-Content "$run/$name.log" -Raw
        if ($result -notmatch 'PASS') {throw "$name did not report successful verification"}
        Write-Output "PASS $name"
    }
    Verify 'checkpoint-preview' {& tools/verify-taoot-route-preview.ps1}
    Verify 'inventory' {& tools/verify-inventory-ui.ps1}
    Verify 'recovery' {& tools/verify-recovery.ps1}
    Verify 'heard-history' {& tools/verify-heard-history.ps1}
    Verify 'accessibility' {& tools/verify-dialogue-accessibility.ps1 -CollectOnly}
    Verify 'leads' {& tools/verify-journal-leads.ps1 -CollectOnly -EvidenceDirectory build/journal-leads/earned}
    Verify 'navigation' {& tools/verify-navigation.ps1 -CollectOnly}
    Verify 'ending-hints' {& tools/verify-ending-hints.ps1 -CollectOnly}
    $binary = Get-Item build/scummvm/Release/scummvm.exe
    foreach ($edition in @('full','demo','dust')) {
        foreach ($kind in @('script','stage','puppet')) {
            $log = Get-Item "build/$kind-$edition.log"
            if ($log.LastWriteTimeUtc -lt $binary.LastWriteTimeUtc) {throw "$edition $kind evidence predates this executable"}
            Copy-Item $log.FullName "$run/$edition-$kind.log"
        }
        $speech = Get-Content "$run/$edition-puppet.log" -Raw
        foreach ($marker in @('HEARD REPLAY PASS','HEARD AUTHORED PASS','DIALOGUE LINE SKIP HOST PASS','GAMEPAD X LINE SKIP PASS','ENDING HINT UI PASS')) {if ($speech -notmatch $marker) {throw "$edition missing $marker"}}
    }
    @{passed=$true; binarySha256=(Get-FileHash $binary.FullName -Algorithm SHA256).Hash; physicalDeviceTested=$false; verificationScope='All eight optional modernization milestones; native Windows desktop and 480x272 controller/pointer interfaces, meaningful earned-state restoration and current executable regressions. This does not certify complete game coverage or physical handheld usability.'; limitations=@('Physical Android, iOS, PSP and handheld/controller runtime remain untested','Navigation uses actual spoken encounter positions, not live character tracking','Hints are curated discovered-information reminders, not an exhaustive walkthrough','Original Mac game data remain unsupported; platform builds use supplied English Windows data')} | ConvertTo-Json -Depth 4 | Set-Content "$run/proof.json"
    Write-Output "All eight modernization milestones PASS: $run"
} finally {Pop-Location}
