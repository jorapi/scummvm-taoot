# SPDX-License-Identifier: GPL-3.0-or-later
# One earned retail best-ending route, with every resumed save linked to fresh boot.
param([ValidateSet('mission1','telegram','painting','evidence','ending')][string]$StartAt='mission1', [switch]$SkipSpeech, [string[]]$OptionalChoices=@('jones1.pup:101'), [string]$EvidenceStartAt='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
$previousSpeech=$env:DREAMFACTORY_BOOT_SKIP_SPEECH
$previousOptional=$env:DREAMFACTORY_BOOT_OPTIONAL_CHOICES
$manifest=$null
try {
    if ($SkipSpeech) { $env:DREAMFACTORY_BOOT_SKIP_SPEECH='1' }
    $env:DREAMFACTORY_BOOT_OPTIONAL_CHOICES=$OptionalChoices -join ','
    $run=Join-Path $root ('build/full-walkthrough/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    function Get-WalkthroughHashes {
        $files=@(Get-ChildItem "$root/scummvm/engines/dreamfactory" -File -Recurse; Get-ChildItem "$root/tools" -File)
        @($files | Sort-Object FullName | ForEach-Object {
            @{ path=[IO.Path]::GetRelativePath($root,$_.FullName); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        })
    }
    $initial=Get-WalkthroughHashes
    $binary=(Get-FileHash "$root/build/scummvm/Release/scummvm.exe" -Algorithm SHA256).Hash
    $manifest=[ordered]@{ scope='One retail best-ending walkthrough with native player input and checkpoint continuity; other branches need separate evidence.'; started=(Get-Date).ToUniversalTime().ToString('o'); startAt=$StartAt; evidenceStartAt=$EvidenceStartAt; skipSpeech=$SkipSpeech.IsPresent; binarySha256=$binary; sourceHashes=$initial; stages=@(); passed=$false }
    $jobs=@(
        @{ name='mission1'; file='verify-full-mission1.ps1'; arguments=@{ ExtraActions=@('savecheckpoint') }; copyRoot=$true },
        @{ name='telegram'; file='verify-full-telegram.ps1'; arguments=@{ RestoreCheckpoint="$root/build/save-full-walkthrough-mission2.dfsv" } },
        @{ name='painting'; file='verify-full-painting.ps1'; arguments=@{ RestoreCheckpoint="$root/build/save-full-thayer-sent.dfsv" } },
        @{ name='evidence'; file='verify-full-best-evidence.ps1'; arguments=@{ Stage='all'; StartAt=$EvidenceStartAt } },
        @{ name='ending'; file='verify-full-best-ending.ps1'; arguments=@{ RestoreCheckpoint="$root/build/save-full-best-sinking-phase1.dfsv" } }
    )
    $started=$false
    foreach ($job in $jobs) {
        if (!$started) { if ($job.name -eq $StartAt) { $started=$true } else { continue } }
        $stageStart=Get-Date
        $dest=Join-Path $run $job.name
        New-Item -ItemType Directory -Path $dest | Out-Null
        $arguments=$job.arguments
        & "$root/tools/$($job.file)" @arguments *> "$dest/suite.log"
        if ($job.copyRoot) { Copy-Item -LiteralPath "$root/build/checkpoint-full.dfsv" -Destination "$root/build/save-full-walkthrough-mission2.dfsv" }
        # An interrupted launch has no completed record and cannot prove a stage.
        $records=@(Get-ChildItem "$root/build/trace-history" -Directory -Filter '*-full-*' | ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -File -Filter 'record.json' } | Where-Object { $_.LastWriteTime -ge $stageStart } | Sort-Object LastWriteTime)
        if (!$records.Count) { throw "No archived native records for $($job.name)" }
        $target=$records[-1].FullName
        & python "$root/tools/audit-trace-lineage.py" "$root/build/trace-history" $target *> "$dest/lineage.json"
        if ($LASTEXITCODE) { throw "Unproven fresh-boot checkpoint ancestry: $($job.name); inspect $dest/lineage.json" }
        $manifest.stages += @{ name=$job.name; passed=$true; target=$target; proof="$dest/lineage.json"; records=@($records.FullName) }
        $manifest | ConvertTo-Json -Depth 8 | Set-Content "$run/manifest.json"
        Write-Output "Retail walkthrough $($job.name) and fresh-boot ancestry PASS: $dest"
    }
    $final=Get-WalkthroughHashes
    if (($initial | ConvertTo-Json -Depth 4 -Compress) -cne ($final | ConvertTo-Json -Depth 4 -Compress) -or $binary -ne (Get-FileHash "$root/build/scummvm/Release/scummvm.exe" -Algorithm SHA256).Hash) { throw 'Sources, tools or executable changed during walkthrough' }
    $manifest.passed=$true
    Write-Output "Retail startup-through-best-ending walkthrough PASS: $run"
} finally {
    if ($manifest) {
        $manifest['finished']=(Get-Date).ToUniversalTime().ToString('o')
        $manifest | ConvertTo-Json -Depth 8 | Set-Content "$run/manifest.json"
    }
    $env:DREAMFACTORY_BOOT_SKIP_SPEECH=$previousSpeech
    $env:DREAMFACTORY_BOOT_OPTIONAL_CHOICES=$previousOptional
    Pop-Location
}
