# SPDX-License-Identifier: GPL-3.0-or-later
param([switch]$CollectOnly, [string]$EvidenceDirectory = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if ($CollectOnly -and !$EvidenceDirectory) {throw 'CollectOnly requires an existing EvidenceDirectory'}
    $run = if ($EvidenceDirectory) {[IO.Path]::GetFullPath($EvidenceDirectory)} else {Join-Path $root ('build/journal-leads/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))}
    New-Item -ItemType Directory -Force $run | Out-Null
    if (!$CollectOnly) {
        if (!(Test-Path build/save-dust-pages.dfsv)) {throw 'Earn the Dust schoolhouse-pages checkpoint first'}
        & tools/verify-scripts.ps1 -Puppets *> "$run/puppets.log"
        & tools/verify-scripts.ps1 -Stages *> "$run/stages.log"
        & tools/verify-full-gym.ps1 -ExtraActions @('expectlead rubaiyat','expectnolead painting','expectnolead notebook','savecheckpoint') *> "$run/full-briefing.log"
        Copy-Item build/checkpoint-full.dfsv "$run/full-briefing.dfsv"
        Copy-Item build/boot-full.log "$run/full-native.log"
        & tools/verify-demo-mission.ps1 -ExtraActions @('expectlead photograph','expectnolead notebook','savecheckpoint') *> "$run/demo-briefing.log"
        Copy-Item build/checkpoint-demo.dfsv "$run/demo-briefing.dfsv"
        Copy-Item build/boot-demo.log "$run/demo-native.log"
        & tools/verify-dust-jones-ring-story.ps1 *> "$run/dust-request.log"
        Copy-Item build/save-dust-jones-ring-story.dfsv "$run/dust-request.dfsv"
        & tools/verify-dust-ring-to-marie.ps1 -RestoreCheckpoint "$run/dust-request.dfsv" *> "$run/dust-ring-marie.log"
        Copy-Item build/save-dust-ring-marie.dfsv "$run/dust-ring-marie.dfsv"
        & tools/verify-dust-ring-return.ps1 -RestoreCheckpoint "$run/dust-ring-marie.dfsv" *> "$run/dust-ring-return.log"
        Copy-Item build/save-dust-ring-returned.dfsv "$run/dust-ring-returned.dfsv"
        & tools/verify-dust-ring-jones.ps1 -RestoreCheckpoint "$run/dust-ring-returned.dfsv" *> "$run/dust-ring-delivered.log"
        Copy-Item build/save-dust-ring-jones.dfsv "$run/dust-ring-delivered.dfsv"
    }
    $binary = Get-Item build/scummvm/Release/scummvm.exe
    foreach ($edition in @('full','demo','dust')) {
        foreach ($kind in @('puppet','stage')) {
            $log = Get-Item "build/$kind-$edition.log"
            if ($log.LastWriteTimeUtc -lt $binary.LastWriteTimeUtc) {throw "$edition $kind evidence predates the current executable"}
            Copy-Item $log.FullName "$run/$edition-$kind.log"
        }
        $speech = Get-Content "$run/$edition-puppet.log" -Raw
        foreach ($marker in @('JOURNAL LEADS PASS','JOURNAL LEAD UI PASS','JOURNAL POINTER PASS','ROUTE UI PASS')) {if ($speech -notmatch $marker) {throw "$edition missing $marker"}}
        $state = Get-Content "$run/$edition-stage.log" -Raw
        foreach ($marker in @('JOURNAL SAVE PASS','ROUTE INVENTORY PASS','ROUTE SAVE PASS')) {if ($state -notmatch $marker) {throw "$edition missing $marker"}}
        foreach ($size in @('','compact-')) {
            foreach ($view in @('journal-leads','journal-lead-reader','journal-lead-hint')) {
                Copy-Item "build/puppet-session-$edition/$size$view.bmp" "$run/$edition-$size$view.bmp"
            }
        }
    }
    foreach ($case in @(
        @{edition='full'; file='full-briefing'; lead='rubaiyat'; absent='notebook'},
        @{edition='demo'; file='demo-briefing'; lead='photograph'; absent='notebook'},
        @{edition='dust'; file='dust-request'; lead='ring'; absent='objects'},
        @{edition='dust'; file='dust-ring-delivered'; lead='ring'; absent='objects'}
    )) {
        & tools/trace-boot.ps1 -Editions $case.edition -RestoreCheckpoint "$run/$($case.file).dfsv" -Actions @("expectlead $($case.lead)","expectnolead $($case.absent)",'dumpleads') *> "$run/$($case.file)-restore.log"
        Copy-Item "build/boot-$($case.edition).log" "$run/$($case.file)-restore-native.log"
    }
    $delivered = Get-Content "$run/dust-ring-delivered-restore-native.log" -Raw
    if ($delivered -notmatch 'BOOT LEAD id=ring .*status=Delivered to Jones') {throw 'Earned delivery is not reflected in the journal'}
    # Restore the earlier snapshot again after the later discovery in this profile.
    & tools/trace-boot.ps1 -Editions dust -RestoreCheckpoint "$run/dust-request.dfsv" -Actions @('expectlead ring','expectowner ring fear','expectglobal jonesringstory 1','dumpleads') *> "$run/dust-rewind.log"
    Copy-Item build/boot-dust.log "$run/dust-rewind-native.log"
    $rewound = Get-Content "$run/dust-rewind-native.log" -Raw
    if ($rewound -notmatch 'BOOT LEAD id=ring .*status=Promise made' -or $rewound -match 'status=Delivered to Jones') {throw 'Journal promise status did not rewind with ownership'}
    $snapshots = @{}
    foreach ($name in @('full-briefing','demo-briefing','dust-request','dust-ring-delivered')) {$snapshots[$name] = (Get-FileHash "$run/$name.dfsv" -Algorithm SHA256).Hash}
    @{passed=$true; binarySha256=(Get-FileHash $binary.FullName -Algorithm SHA256).Hash; nativeSnapshots=$snapshots; physicalDeviceTested=$false; goalComplete=$false; scope='Native desktop/480x272 controller tracking, explicit hint reader, pointer scrolling, replay, discovery gates, native save compatibility and authored Titanic/demo briefings; Dust accepted ring promise, actual item exchanges, delivery and earlier-state restore. Catalog is curated, not an exhaustive automatic quest log.'} | ConvertTo-Json -Depth 4 | Set-Content "$run/proof.json"
    Write-Output "Journal lead proof PASS: $run"
} finally {Pop-Location}
