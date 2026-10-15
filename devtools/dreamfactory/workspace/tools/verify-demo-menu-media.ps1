# SPDX-License-Identifier: GPL-3.0-or-later
# Play the supplied demo's menu movies, including their final interactive image.
param([ValidateSet('tour','trailer')][string[]]$Movies = @('tour','trailer'), [string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-demo-replay-menu.dfsv", [int]$TimeoutSeconds = 420)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
foreach ($movie in $Movies) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions demo -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$root/build/save-demo-$movie-return.dfsv" -TimeoutSeconds $TimeoutSeconds -MovieClicks @("$movie.mov:#1") -Actions @(
        "clickon button $movie", "waitpropview porthole $($movie)close 60000",'expectflat demo 1','expectset none','savecheckpoint'
    )
    Copy-Item "$root/build/boot-demo.log" "$root/build/demo-$movie-menu-native.log"
    $log = Get-Content "$root/build/boot-demo.log" -Raw
    if ($log -notmatch "MOVIE PLAY $movie.mov " -or $log -notmatch 'BOOT MOVIE CLICK' -or $log -notmatch 'BOOT READY') { throw "$movie did not complete its menu movie" }
    & "$PSScriptRoot/trace-boot.ps1" -Editions demo -RestoreCheckpoint "$root/build/save-demo-$movie-return.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @('expectset none','expectflat demo 1',"expectpropview porthole $($movie)close")
    Copy-Item "$root/build/boot-demo.log" "$root/build/demo-$movie-menu-restore-native.log"
    Write-Output "Demo $movie movie, final image click, menu return and fresh restore PASS"
}
