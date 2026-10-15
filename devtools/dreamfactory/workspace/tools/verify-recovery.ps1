# SPDX-License-Identifier: GPL-3.0-or-later
# Real earned saves, original turning/trading and native ScummVM autosaves.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/recovery/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    function Test-RecoveryCase([string]$Name, [hashtable]$Parameters) {
        $Parameters.SavePath = "$run/$Name/saves"
        $Parameters.Checkpoint = "$run/$Name.dfsv"
        $Parameters.SkipSpeech = $true
        $Parameters.Steps = 1
        $Parameters.TimeoutSeconds = 180
        & tools/trace-boot.ps1 @Parameters *> "$run/$Name.log"
        Copy-Item -LiteralPath "build/boot-$($Parameters.Editions).log" -Destination "$run/$Name-native.log"
        Write-Output "PASS $Name"
    }
    foreach ($compact in @($false, $true)) {
        $size = if ($compact) {'compact'} else {'desktop'}
        foreach ($edition in @('full','demo','dust')) {
            $save = switch ($edition) {full {'save-full-best-sinking-phase1'} demo {'save-demo-opening'} dust {'save-dust-day5-morning'}}
            $original = switch ($edition) {full {'View250'} demo {'View55'} dust {'south'}}
            $turned = switch ($edition) {full {'View251'} demo {'View52'} dust {'north'}}
            $state = switch ($edition) {full {@('expectglobal mission 4','expectglobal phase 1','expectowner painting frank','expectowner rubaiyat frank','expectowner realneck frank','expectowner notebook frank')} demo {@('expectowner trunkkey frank')} dust {@('expectglobal day 5','expectowner chest stranger')}}
            $keys = @('snapshot','pad:a','snapshot','pad:a','pad:b','pad:down','pad:down','snapshot','pad:a','pad:up','pad:a')
            $actions = @("expectview $original",'expectrecoveries 0','saveslot 7','recoverysave','key rightarrow','recoverysave','key rightarrow','recoverysave','expectrecoveries 3',"expectview $turned",'openrecovery',"expectview $turned",'openrecovery',"expectview $original") + $state + @('expectsave 7 DreamFactory checkpoint test','recoverysave','expectrecoveries 3')
            Test-RecoveryCase "$edition-$size" @{Editions=$edition; RestoreCheckpoint="build/$save.dfsv"; RecoveryCompact=$compact; GuiKeys=$keys; Actions=$actions}
        }
    }
    Test-RecoveryCase 'pointer-compact' @{Editions='full'; RestoreCheckpoint='build/save-full-best-sinking-phase1.dfsv'; RecoveryCompact=$true; GuiKeys=@('point:80:180','snapshot','point:40:245','point:40:245'); Actions=@('recoverysave','key rightarrow','recoverysave','key rightarrow','recoverysave','expectview View251','openrecovery','expectview View250')}
    Test-RecoveryCase 'dust-trade' @{Editions='dust'; RestoreCheckpoint='build/save-dust-blood-hotel.dfsv'; RecoveryCompact=$true; InventoryCompact=$true; GuiKeys=@('pad:up','pad:a','snapshot','pad:a','pad:up','pad:a'); Choices=@('fear.pup:101'); Actions=@('expectowner ring stranger','rememberglobal phase','rememberglobal playercash','rememberglobal day','rememberglobal clock','recoverysave','recoverysave','recoverysave','openinventory','expectglobal handitem ring','padon actor fear','expectowner ring fear','expectglobal phase 5','expectglobal playercash 6','openrecovery','expectowner ring stranger','expectglobal handitem cigar','expectremembered phase','expectremembered playercash','expectremembered day','expectremembered clock','expectrecoveries 3')}
    Test-RecoveryCase 'interval' @{Editions='demo'; RestoreCheckpoint='build/save-demo-opening.dfsv'; AutosaveSeconds=1; Actions=@('waitrecoveries 2','expectrecoveries 2')}
    Test-RecoveryCase 'empty' @{Editions='demo'; RestoreCheckpoint='build/save-demo-opening.dfsv'; RecoveryCompact=$true; GuiKeys=@('pad:a','snapshot','pad:b'); Actions=@('expectrecoveries 0','openrecovery','expectview View55')}
    # Opening recovery in a fresh process must retain its three finalized files.
    & tools/trace-boot.ps1 -Editions full -RestoreCheckpoint build/save-full-best-sinking-phase1.dfsv -SavePath "$run/full-compact/saves" -Checkpoint "$run/restart.dfsv" -RecoveryCompact -GuiKeys @('snapshot','pad:b','snapshot','pad:a','pad:up','pad:a') -Actions @('expectrecoveries 3','openrecovery','freshcontinue','expectview View250') *> "$run/restart.log"
    $damagedFile = Get-ChildItem "$run/full-compact/saves" -Filter '*.dfrecovery-*.3' | Select-Object -First 1
    if (!$damagedFile) {throw 'Expected finalized recovery generation missing'}
    $intact = [IO.File]::ReadAllBytes($damagedFile.FullName)
    try {
        [IO.File]::WriteAllBytes($damagedFile.FullName, [byte[]]@(0))
        & tools/trace-boot.ps1 -Editions full -RestoreCheckpoint build/save-full-best-sinking-phase1.dfsv -SavePath "$run/full-compact/saves" -Checkpoint "$run/corrupt.dfsv" -RecoveryCompact -GuiKeys @('snapshot','pad:a','pad:up','pad:a') -Actions @('expectrecoveries 2','openrecovery','expectview View251') *> "$run/corrupt.log"
    } finally { [IO.File]::WriteAllBytes($damagedFile.FullName, $intact) }
    @{passed=$true; binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash; scope='Three native recovery generations, controller confirmation/cancel and older-state restore at desktop/480x272 for all editions, pointer selection, real Dust ownership/quest/cash/clock rewind, standard autosave interval and fresh-process persistence. Physical devices remain untested.'} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Recovery native proof PASS: $run"
} finally {Pop-Location}
