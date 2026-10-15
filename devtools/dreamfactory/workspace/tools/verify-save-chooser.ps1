# SPDX-License-Identifier: GPL-3.0-or-later
# Exercise real ScummVM choosers through the authored controls, with physical keys.
param([int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$run = [Guid]::NewGuid().ToString('N')
foreach ($edition in 'full','demo','dust') {
    $checkpoint = if ($edition -eq 'dust') { 'day5-morning' } else { 'opening' }
    $savedir = "$root/build/chooser-saves-$run-$edition"
    $view = switch ($edition) { full { 'View14' } demo { 'View55' } dust { 'south' } }
    $turned = switch ($edition) { full { 'View16' } demo { 'View71' } dust { 'west' } }
    if ($edition -eq 'dust') {
        $actions = @('clickon button horn','expectflat score',
            'click 252 196','expectflat score','click 252 228','expectflat score',
            'click 252 196','expectflat score','expectsave 1 a',
            'click 252 322','expectflat mainpanel','key rightarrow',"expectview $turned",
            'clickon button horn','expectflat score','click 252 228','expectflat mainpanel')
    } else {
        $actions = @('clickon prop life')
        if ($edition -eq 'full') { $actions += 'clickon prop life' }
        $actions += @('expectflat ctl 1','click 130 48','expectflat ctl 1',
            'click 130 104','expectflat ctl 1','click 130 48','expectflat ctl 1',
            'expectsave 1 a','click 130 328','expectflat main 1',
            'key rightarrow',"expectview $turned",'clickon prop life',
            'expectflat ctl 1','click 130 104','expectflat main 1')
    }
    $actions += @("expectview $view",'key rightarrow',"expectview $turned")
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-$checkpoint.dfsv" -SavePath $savedir -TimeoutSeconds $TimeoutSeconds -GuiKeys @(
        'escape','escape','home','down','enter','a','enter','home','down','enter'
    ) -Actions $actions
    Copy-Item "$root/build/boot-$edition.log" "$root/build/chooser-$edition-native.log"
    if (!(Test-Path "$savedir/audit.001")) { throw "$edition chooser did not create slot 1" }
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-$checkpoint.dfsv" -SavePath $savedir -TimeoutSeconds $TimeoutSeconds -Actions @(
        'expectsave 1 a','loadslot 1',"expectview $view",'key rightarrow',"expectview $turned"
    )
    Copy-Item "$root/build/boot-$edition.log" "$root/build/chooser-$edition-fresh-native.log"
    # Also use F5's Save/Load controls, rather than the game's own panel.
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-$checkpoint.dfsv" -SavePath $savedir -TimeoutSeconds $TimeoutSeconds -GuiKeys @(
        's','home','down','down','enter','b','enter','l','home','down','down','enter'
    ) -Actions @('menugui','expectsave 2 b','key rightarrow',"expectview $turned",'menugui',"expectview $view",'key rightarrow',"expectview $turned")
    Copy-Item "$root/build/boot-$edition.log" "$root/build/chooser-$edition-menu-native.log"
    Write-Output "$edition real chooser cancellation, named save, listed metadata, load and fresh restore PASS; saves=$savedir"
}
