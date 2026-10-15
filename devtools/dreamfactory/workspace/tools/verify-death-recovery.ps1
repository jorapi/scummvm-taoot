# SPDX-License-Identifier: GPL-3.0-or-later
# Earned deaths, actual New Game/Load controls and fresh-process recovery.
param([ValidateSet('dust','full')][string[]]$Editions = @('dust','full'), [switch]$CreateDeaths, [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$run = [Guid]::NewGuid().ToString('N')
foreach ($edition in $Editions) {
    $death = if ($edition -eq 'dust') { "$root/build/save-dust-kid-death.dfsv" } else { "$root/build/save-full-vlad-death.dfsv" }
    if ($CreateDeaths) {
        if ($edition -eq 'full') { & "$PSScriptRoot/verify-full-vlad-death.ps1" -TimeoutSeconds $TimeoutSeconds }
        else {
            $route = @(& node "$PSScriptRoot/dust-route.mjs" TOWN G5 south G9 south)
            if ($LASTEXITCODE) { throw 'Cannot plan the gang firing-range approach' }
            & "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-kid-wave3-active.dfsv" -Checkpoint $death -TimeoutSeconds $TimeoutSeconds -Actions ($route + @('waitset none 60000','expectflat death','savecheckpoint'))
            Copy-Item "$root/build/boot-dust.log" "$root/build/dust-kid-death-native.log"
        }
    }
    if ($edition -eq 'dust') {
        $restart = @('expectset none','expectflat death','clickon button new',
            'expectset town','expectscene Scene G15','expectview north','expectglobal day 1',
            'expectglobal clock 3','expectglobal phase 0','expectglobal playercash 5','expectglobal bulletcount 6',
            'expectglobal fighton 0','expectowner tbird none','expectflat mainpanel',
            'key rightarrow','expectview east','key leftarrow','expectview north')
        $restartCheck = @('expectset town','expectglobal day 1','expectglobal clock 3','expectglobal playercash 5',
            'expectglobal fighton 0','expectscene Scene G15','expectview north','key rightarrow','expectview east')
        $source = "$root/build/save-dust-day5-morning.dfsv"
        $recover = @('expectflat death','clickon button open','expectflat death','expectset none',
            'clickon button open','expectflat mainpanel','expectglobal day 5','expectglobal clock 1',
            'expectowner chest stranger','expectview south','key rightarrow','expectview west')
        $recoveredCheck = @('expectflat mainpanel','expectglobal day 5','expectowner chest stranger',
            'expectview west','key leftarrow','expectview south')
    } else {
        $restart = @('expectset none','expectflat ctl 1','expectglobal playerdeath by vlad','click 130 48',
            'expectset bedsit1','expectview View14','expectflat main 1','expectglobal mission 0','expectglobal phase 0',
            'expectglobal clock bedsit','expectglobal playerdeath ','expectowner rubaiyat none','expectowner vladfood none',
            'key rightarrow','expectview View16','key leftarrow','expectview View14')
        $restartCheck = @('expectset bedsit1','expectglobal mission 0','expectglobal clock bedsit','expectglobal playerdeath ',
            'expectowner rubaiyat none','expectview View14','key rightarrow','expectview View16')
        $source = "$root/build/save-full-best-sinking-phase1.dfsv"
        $recover = @('expectset none','expectflat ctl 1','click 130 104','expectset none','expectflat ctl 1',
            'expectglobal playerdeath by vlad','click 130 104','expectset deckbd2','expectflat main 1',
            'expectglobal mission 4','expectglobal phase 1','expectglobal playerdeath ',
            'expectowner painting frank','expectowner rubaiyat frank','expectowner realneck frank','expectowner notebook frank',
            'key rightarrow','key leftarrow','expectview View250')
        $recoveredCheck = @('expectset deckbd2','expectglobal mission 4','expectglobal playerdeath ',
            'expectowner notebook frank','expectview View250','key rightarrow','key leftarrow','expectview View250')
    }
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint $death -Checkpoint "$root/build/save-$edition-restarted.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions ($restart + @('savecheckpoint'))
    Copy-Item "$root/build/boot-$edition.log" "$root/build/$edition-death-restart-native.log"
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-restarted.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions $restartCheck
    Copy-Item "$root/build/boot-$edition.log" "$root/build/$edition-death-restart-restore-native.log"
    $savedir = "$root/build/death-saves-$run-$edition"
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint $source -SavePath $savedir -TimeoutSeconds $TimeoutSeconds -Actions @('saveslot 1','expectsave 1 DreamFactory checkpoint test')
    Copy-Item "$root/build/boot-$edition.log" "$root/build/$edition-death-recovery-save-native.log"
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint $death -Checkpoint "$root/build/save-$edition-death-recovered.dfsv" -SavePath $savedir -TimeoutSeconds $TimeoutSeconds -GuiKeys @('escape','home','down','enter') -Actions ($recover + @('savecheckpoint'))
    Copy-Item "$root/build/boot-$edition.log" "$root/build/$edition-death-load-native.log"
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-death-recovered.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions $recoveredCheck
    Copy-Item "$root/build/boot-$edition.log" "$root/build/$edition-death-load-restore-native.log"
    Write-Output "$edition death-screen restart, cancelled Load, late-game recovery and fresh restores PASS"
}
