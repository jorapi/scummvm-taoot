# SPDX-License-Identifier: GPL-3.0-or-later
# Check the ordinary launcher entry point, without gameplay or boot-trace flags.
# This startup probe stops its own process at the authored menu or after BOOTFILE.
param([ValidateSet('demo','full','dust')][string[]]$Editions = @('demo','full','dust'), [int]$TimeoutSeconds = 360, [int]$SaveSlot = -1, [string]$ExpectedRoom = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$oldVideo = $env:SDL_VIDEODRIVER
$oldAudio = $env:SDL_AUDIODRIVER
try {
    $env:SDL_VIDEODRIVER = 'dummy'
    $env:SDL_AUDIODRIVER = 'dummy'
    foreach ($edition in $Editions) {
        $game = if ($edition -eq 'dust') { 'dust' } else { 'taoot' }
        $config = "$root/build/launch-$edition.ini"
        $log = "$root/build/launch-$edition.log"
        @"
[scummvm]
enable_unsupported_game_warning=false
gui_theme=builtin
gfx_mode=surfacesdl
joystick_num=-1
savepath=$root/build/test-saves

[audit]
engineid=dreamfactory
gameid=$game
path=$root/game-data/$edition
language=en
platform=windows
extrapath=$root/build/runtime-data
dreamfactory_font=$root/build/runtime-data/LiberationSans-Regular.ttf
"@ | Set-Content $config
        if (Test-Path $log) { Remove-Item -LiteralPath $log }
        $nativeArgs = @("--config=`"$config`"", "--logfile=`"$log`"", '--debuglevel=1')
        if ($SaveSlot -ge 0) { $nativeArgs += "--save-slot=$SaveSlot" }
        $nativeArgs += 'audit'
        $process = Start-Process -FilePath "$root/build/scummvm/Release/scummvm.exe" -ArgumentList $nativeArgs -WindowStyle Hidden -PassThru
        $watch = [Diagnostics.Stopwatch]::StartNew()
        try {
            $ready = $false
            while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
                if (Test-Path $log) {
                    $text = Get-Content $log -Raw
                    # Retail waits for a real click in its game/tour menu inside
                    # BOOTFILE. Reaching that interactive movie is its ready state.
                    $pattern = if ($ExpectedRoom) { 'GAMEPLAY READY room=' + [regex]::Escape($ExpectedRoom) + ' scene=.* stage=\S+ flat=\S+' } else { 'GAMEPLAY READY room=.* stage=\S+ flat=\S+' }
                    if ($text -match $pattern -or
                        ($SaveSlot -lt 0 -and !$ExpectedRoom -and $edition -eq 'full' -and $text -match 'MOVIE PLAY playmode\.mov')) { $ready = $true; break }
                }
                $process.Refresh()
                if ($process.HasExited) { throw "$edition exited before gameplay startup; inspect $log" }
                Start-Sleep -Milliseconds 500
            }
            if (!$ready) { throw "$edition ordinary startup timed out; inspect $log" }
            Write-Output "$edition default gameplay startup PASS"
        } finally {
            $process.Refresh()
            if (!$process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
            $process.Dispose()
        }
    }
} finally {
    $env:SDL_VIDEODRIVER = $oldVideo
    $env:SDL_AUDIODRIVER = $oldAudio
}
