# SPDX-License-Identifier: GPL-3.0-or-later
param([switch]$CollectOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/ending-hints/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Force $run | Out-Null
    if (!$CollectOnly) {
        & tools/verify-scripts.ps1 *> "$run/scripts.log"
        & tools/verify-scripts.ps1 -Stages *> "$run/stages.log"
        & tools/verify-scripts.ps1 -Puppets *> "$run/puppets.log"
    }
    $binary = Get-Item build/scummvm/Release/scummvm.exe
    foreach ($edition in @('full','demo','dust')) {
        foreach ($kind in @('script','stage','puppet')) {
            $log = Get-Item "build/$kind-$edition.log"
            if ($log.LastWriteTimeUtc -lt $binary.LastWriteTimeUtc) {throw "$edition $kind evidence predates the current executable"}
            Copy-Item $log.FullName "$run/$edition-$kind.log"
        }
        $speech = Get-Content "$run/$edition-puppet.log" -Raw
        if ($speech -notmatch 'ENDING HINTS PASS' -or $speech -notmatch 'ENDING HINT UI PASS' -or [regex]::Matches($speech,'ENDING HINT REQUESTS shown=2 available=3').Count -ne 4) {throw "$edition lacks actual two-step hint progression in all four native readers"}
        foreach ($size in @('','compact-')) {
            foreach ($visibility in @('hidden','reached')) {
                foreach ($view in @('reader','hint1','hint2')) {
                    $capture = "build/puppet-session-$edition/$($size)ending-$visibility-$view.bmp"
                    Copy-Item $capture "$run/$edition-$($size)ending-$visibility-$view.bmp"
                    $header = [IO.File]::ReadAllBytes($capture)
                    if ($size -and ([BitConverter]::ToInt32($header,18) -ne 480 -or [Math]::Abs([BitConverter]::ToInt32($header,22)) -ne 272)) {throw 'Hint reader capture is not a native 480x272 overlay'}
                }
            }
        }
    }
    if ((Get-Content "$run/full-script.log" -Raw) -notmatch 'CHECKPOINT CLOCK PASS.*original calctime.*preview read-only.*native rewind') {throw 'Original watch formatting, snapshot isolation or clock rewind evidence missing'}
    foreach ($case in @(
        @{edition='full'; save='build/navigation-earned/full-briefing.dfsv'; pattern='PENNY1\.PUP.*penny1\.056'},
        @{edition='demo'; save='build/navigation-earned/demo-briefing.dfsv'; pattern='DPENNY\.PUP.*dpen\.(16|17|33)'},
        @{edition='dust'; save='build/journal-leads/earned/dust-request.dfsv'; pattern=''}
    )) {
        & tools/trace-boot.ps1 -Editions $case.edition -RestoreCheckpoint $case.save -SavePath "$run/saves" -Actions @('dumpendinghints') *> "$run/$($case.edition)-earned.log"
        Copy-Item "build/boot-$($case.edition).log" "$run/$($case.edition)-earned-native.log"
        $text = Get-Content "$run/$($case.edition)-earned-native.log" -Raw
        if ($text -notmatch 'BOOT ENDING HINTS') {throw 'Ending reminder diagnostic missing'}
        if ($case.pattern -and $text -notmatch "(?i)BOOT ENDING EVIDENCE file=.*$($case.pattern)") {throw "$($case.edition) actually heard assignment did not unlock its reminder"}
        if ($case.edition -eq 'full' -and $text -match 'BOOT ENDING HINT \d+ .*\b(necklace|notebook|painting)\b') {throw 'Early Titanic briefing leaked a later objective'}
        if ($case.edition -eq 'dust' -and $text -notmatch 'BOOT ENDING HINTS evidence=0') {throw 'An unrelated ring promise unlocked an unseen ending instruction'}
    }
    @{passed=$true; binarySha256=(Get-FileHash $binary.FullName -Algorithm SHA256).Hash; physicalDeviceTested=$false; scope='Explicit current-save ending reminders, no target-slot solution, source and edition gates, hidden-owner privacy, native desktop/480x272 reached and unreached Flow readers, mapped controller and pointer two-step reveals, earned Titanic/demo assignment evidence and unrelated Dust promise exclusion. Exact original watch formatting and native clock rewind.'} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Ending hints proof PASS: $run"
} finally {Pop-Location}
