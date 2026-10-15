# SPDX-License-Identifier: GPL-3.0-or-later
# One complete earned Dust route; every saved stage must descend from fresh boot.
param([string]$StartAt='start', [switch]$SkipSpeech, [ValidateRange(1,10)][int]$CasinoBetSize=1, [ValidateRange(12,20)][int]$CasinoHitBelow=17)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
$previousSpeech=$env:DREAMFACTORY_BOOT_SKIP_SPEECH
$manifest=$null
try {
    if ($SkipSpeech) { $env:DREAMFACTORY_BOOT_SKIP_SPEECH='1' }
    $run=Join-Path $root ('build/dust-walkthrough/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    function Get-WalkthroughHashes {
        $files=@(Get-ChildItem "$root/scummvm/engines/dreamfactory" -File -Recurse; Get-ChildItem "$root/tools" -File)
        @($files | Sort-Object FullName | ForEach-Object { @{ path=[IO.Path]::GetRelativePath($root,$_.FullName); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash } })
    }
    function Route($Room,$From,$View,$To,$Facing) {
        $r=@(& node "$root/tools/dust-route.mjs" $Room $From $View $To $Facing)
        if($LASTEXITCODE) { throw 'Cannot plan Dust walkthrough movement' }
        $r
    }
    function Trace($Source,$Output,$Actions) {
        & "$root/tools/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/$Source.dfsv" -Checkpoint "$root/build/$Output.dfsv" -Steps 1 -TimeoutSeconds 300 -Actions $Actions
    }
    $initial=Get-WalkthroughHashes
    $binary=(Get-FileHash "$root/build/scummvm/Release/scummvm.exe" -Algorithm SHA256).Hash
    $manifest=[ordered]@{ scope='One Dust startup-through-Yunni-ending route using native player input and earned, linked checkpoints; optional branches have separate checks.'; started=(Get-Date).ToUniversalTime().ToString('o'); startAt=$StartAt; skipSpeech=$SkipSpeech.IsPresent; binarySha256=$binary; sourceHashes=$initial; stages=@(); passed=$false }
    $steps=@(
        'start','trotter-intro','trotter','cash-entry','cash','night-street','blood-hotel','hotel-ring','hotel-key','night-warning','night-wine','first-night',
        'day2-wakeup','day2-breakfast','breakfast-items','day2-livery','well-gun','pie','boots','day2-afternoon','handkerchief','handkerchief-chit','buick-wine','blood-skooch','laurel-bullets','bank-deposit','flippo-alarm','cobb','sheriff','night2-laurel','second-night',
        'day3-wakeup','day3-breakfast','day3-range-entry','range','matchbox-in-desk','pages','jones-ring-story','ring-to-marie','ring-return','ring-jones','day3-afternoon','yunni-book','prescription','prescription-watson','medicine','flute','mansion-entry','mask','day3-night','safe-entry','tstone','night3-jones','night3-reconcile','night3-poker-offer','bounty-entry','bounty','tbird','third-night',
        'day4-morning','kid-fight-entry','equip-gun','kid-wave1','wave2-entry','kid-wave2','kid-wave3','kid-duel','temple-box-entry','tstone-box','hub-entry','sundial-entry','flute-puzzle','sundial-after-flute','snake-entry','snake-puzzle','sundial-after-snake','tbird-puzzle-entry','tbird-puzzle','sundial-after-tbird','mine-entry','mine-puzzle','shaman-chest','blood-finale-entry','blood-finale','ending'
    )
    if($StartAt -notin $steps) { throw "Unknown Dust stage: $StartAt" }
    $started=$false
    foreach($step in $steps) {
        if(!$started) { if($step -eq $StartAt) { $started=$true } else { continue } }
        $stageStart=Get-Date
        $dest=Join-Path $run $step
        New-Item -ItemType Directory -Path $dest | Out-Null
        & {
            switch($step) {
                start {
                    & "$root/tools/verify-dust-saloon.ps1" -ExtraActions @('expectscene Scene D1','expectview west','savecheckpoint')
                    Copy-Item -LiteralPath "$root/build/checkpoint-dust.dfsv" -Destination "$root/build/save-dust-walkthrough-saloon-entry.dfsv"
                }
                trotter-intro {
                    Trace save-dust-walkthrough-saloon-entry save-dust-trotter-intro ((Route SALLOWER D1 west C4 north)+@('clickon actor trotter','expectglobal trotterphase 1','savecheckpoint'))
                }
                cash-entry {
                    Trace save-dust-trotter-departure save-dust-walkthrough-cash-entry @('expectscene Scene D3','expectview west','key uparrow','expectscene Scene C3','key leftarrow','key uparrow','expectscene Scene C4','key rightarrow','expectview west','savecheckpoint')
                }
                cash {
                    & "$root/tools/verify-dust-blackjack-cash.ps1" -RestoreCheckpoint "$root/build/save-dust-walkthrough-cash-entry.dfsv" -MinimumCash 6 -MaximumHands 32 -BetSize $CasinoBetSize -HitBelow $CasinoHitBelow
                }
                bounty-entry {
                    $a=(Route SALLOWER C2 east D1 east)+@('click 250 150','expectowner door salout','key uparrow','expectset town')+(Route NITE G8 east G11 north)+@('expectglobal fighton 1','idle 80','expectglobal phase 3','expectglobal playerdeath ','savecheckpoint')
                    Trace save-dust-night3-poker-offer save-dust-bounty-active $a
                }
                bounty { & "$root/tools/verify-dust-bounty-live.ps1" }
                kid-wave1 { & "$root/tools/verify-dust-bounty-live.ps1" -GangWave1 -Armed -RestoreCheckpoint "$root/build/save-dust-kid-fight-active.dfsv" }
                equip-gun {
                    $ammo=& python "$root/tools/read-checkpoint-globals.py" "$root/build/save-dust-kid-fight-entry.dfsv" bulletcount | ConvertFrom-Json
                    if($LASTEXITCODE -or $ammo.bulletcount -lt 0 -or $ammo.bulletcount -gt 6) { throw 'Cannot read ammunition before the Kid fight' }
                    $a=@('clickon prop avatar','expectflat avatar','clickon prop gun','clickon button ok','expectglobal handitem gun','clickon prop gun','waitpropview gunhand idle 10000')
                    $missing=6-[int]$ammo.bulletcount
                    if($missing) {
                        $a+=@('clickon prop gunhand','expectpropview gunhand reload')
                        for($bullet=0;$bullet -lt $missing;$bullet++) { $a+='click 234 235' }
                        $a+=@('clickon prop gunhand','expectpropview gunhand idle')
                    }
                    $a+=@('expectglobal bulletcount 6','idle 60','expectglobal playerdeath ','savecheckpoint')
                    Trace save-dust-kid-fight-entry save-dust-kid-fight-active $a
                }
                wave2-entry {
                    $native=Get-Content "$root/build/dust-kid-wave1-complete-native.log" -Raw
                    $place=[regex]::Match($native,'BOOT LOCATION room=town scene=(.*?) view=(.*?) stage=')
                    if(!$place.Success) { throw 'Missing earned first-wave exit standpoint' }
                    Trace save-dust-kid-wave1-defeated save-dust-kid-wave2-probe ((Route TOWN $place.Groups[1].Value $place.Groups[2].Value G11 south)+@('idle 40','expectglobal fightphase 2','expectglobal playerdeath ','savecheckpoint'))
                }
                sundial-after-flute { & "$root/tools/verify-dust-sundial-entry.ps1" -RestoreCheckpoint "$root/build/save-dust-flute-puzzle-solved.dfsv" -Checkpoint "$root/build/save-dust-sundial-after-flute.dfsv" }
                sundial-after-snake { & "$root/tools/verify-dust-sundial-entry.ps1" -RestoreCheckpoint "$root/build/save-dust-snake-puzzle-solved.dfsv" -Checkpoint "$root/build/save-dust-sundial-after-snake.dfsv" }
                sundial-after-tbird { & "$root/tools/verify-dust-sundial-entry.ps1" -RestoreCheckpoint "$root/build/save-dust-tbird-puzzle-solved.dfsv" -Checkpoint "$root/build/save-dust-sundial-after-tbird.dfsv" }
                ending { & "$root/tools/verify-dust-ending.ps1" -Ending yunni -SkipSpeech:$SkipSpeech }
                default { & "$root/tools/verify-dust-$step.ps1" }
            }
        } *> "$dest/suite.log"
        # Interrupted launches can leave an archive without a completed record.
        # Only completed records are candidates; the lineage auditor validates
        # their success, hashes and ancestry before accepting this stage.
        $records=@(Get-ChildItem "$root/build/trace-history" -Directory -Filter '*-dust-*' | ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -File -Filter 'record.json' } | Where-Object { $_.LastWriteTime -ge $stageStart } | Sort-Object LastWriteTime)
        if(!$records.Count) { throw "No native trace evidence for $step" }
        $target=$records[-1].FullName
        & python "$root/tools/audit-trace-lineage.py" "$root/build/trace-history" $target *> "$dest/lineage.json"
        if($LASTEXITCODE) { throw "Unproven fresh-boot checkpoint ancestry: $step; inspect $dest/lineage.json" }
        $manifest.stages+=@{ name=$step; passed=$true; target=$target; proof="$dest/lineage.json"; records=@($records.FullName) }
        $manifest | ConvertTo-Json -Depth 8 | Set-Content "$run/manifest.json"
        Write-Output "Dust walkthrough $step and fresh-boot ancestry PASS: $dest"
    }
    $final=Get-WalkthroughHashes
    if(($initial | ConvertTo-Json -Depth 4 -Compress) -cne ($final | ConvertTo-Json -Depth 4 -Compress) -or $binary -ne (Get-FileHash "$root/build/scummvm/Release/scummvm.exe" -Algorithm SHA256).Hash) { throw 'Sources, tools or executable changed during walkthrough' }
    $manifest.passed=$true
    Write-Output "Dust startup-through-ending walkthrough PASS: $run"
} finally {
    if($manifest) {
        $manifest['finished']=(Get-Date).ToUniversalTime().ToString('o')
        $manifest | ConvertTo-Json -Depth 8 | Set-Content "$run/manifest.json"
    }
    $env:DREAMFACTORY_BOOT_SKIP_SPEECH=$previousSpeech
    Pop-Location
}
