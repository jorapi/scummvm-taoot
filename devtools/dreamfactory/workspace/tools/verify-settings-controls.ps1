# SPDX-License-Identifier: GPL-3.0-or-later
# Physical game controls and persisted settings; no host calls injected.
param([int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-day5-morning.dfsv" -Checkpoint "$root/build/save-dust-settings-panel.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'clickon button horn','expectflat score','dragpoint prop slider 117 323','expectsetting wavevolume 0',
    'clickon button subtitles','expectsetting subtitles 0','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-settings-panel-native.log"
& "$PSScriptRoot/trace-boot.ps1" -Editions dust -RestoreCheckpoint "$root/build/save-dust-settings-panel.dfsv" -Checkpoint "$root/build/save-dust-settings-restored.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectflat score','expectsetting wavevolume 0','expectsetting subtitles 0',
    'dragpoint prop slider 117 267','expectsetting wavevolume 5','dragpoint prop slider 117 225','expectsetting wavevolume 9',
    'clickon button subtitles','expectsetting subtitles 1','clickon button OK','expectflat mainpanel',
    'key rightarrow','expectview west','key leftarrow','expectview south','savecheckpoint')
Copy-Item "$root/build/boot-dust.log" "$root/build/dust-settings-restore-native.log"
foreach ($edition in 'full','demo') {
    $opening = @('clickon prop life')
    if ($edition -eq 'full') { $opening += 'clickon prop life' }
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-opening.dfsv" -Checkpoint "$root/build/save-$edition-volume-dial.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions ($opening + @(
        'expectflat ctl 1','expectsetting wavevolume 9','expectsetting framerate 3',
        'dragarc prop volume 397 281 -32 8','expectsetting wavevolume 1','expectsetting framerate 3',
        'clickon prop subtoggle','expectsetting subtitles 0',
        'clickon button east','key d','expectglobal keyeast d',
        'click 130 328','expectflat main 1','savecheckpoint'))
    Copy-Item "$root/build/boot-$edition.log" "$root/build/$edition-volume-dial-native.log"
    $view = if ($edition -eq 'full') { 'View14' } else { 'View55' }
    $turned = if ($edition -eq 'full') { 'View16' } else { 'View71' }
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-volume-dial.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
        'expectsetting wavevolume 1','expectsetting framerate 3','expectsetting subtitles 0',
        'expectglobal keyeast d','key d',"expectview $turned",'key leftarrow',"expectview $view",'clickon prop life','expectflat ctl 1',
        'dragarc prop volume 397 281 32 8','expectsetting wavevolume 9','expectsetting framerate 3',
        'clickon prop subtoggle','expectsetting subtitles 1','clickon button east','key 6','expectglobal keyeast 6',
        'click 130 328','expectflat main 1',"expectview $view",'key rightarrow',"expectview $turned")
    Copy-Item "$root/build/boot-$edition.log" "$root/build/$edition-volume-dial-restore-native.log"
}
Write-Output 'Dust volume/subtitles, both Titanic rotary dials, subtitle toggles, custom turn keys and persisted settings PASS'
