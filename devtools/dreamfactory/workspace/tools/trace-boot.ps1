param([string[]]$MovieClicks = @(), [string[]]$ModalClicks = @(), [int]$DialogSlot = -1, [string]$RestoreCheckpoint = "", [string]$Checkpoint = "", [string[]]$Choices = @(), [string[]]$OptionalChoices = @(), [string[]]$Actions = @(), [string[]]$SkipMovies = @(), [string[]]$ClickMovies = @(), [string[]]$Keys = @(), [int]$ClickX = -1, [int]$ClickY = -1, [int]$Steps = 0, [switch]$ChooseGame, [switch]$SkipSpeech = ($env:DREAMFACTORY_BOOT_SKIP_SPEECH -eq '1'), [int]$TimeoutSeconds = 60, [int]$DebugLevel = 1, [ValidateSet('demo','full','dust')][string[]]$Editions = @('demo','full','dust'), [switch]$DefaultFont, [string]$Executable="", [ValidateSet('game','on','off','only')][string]$SubtitleMode='game', [string]$ExtraPath = "", [string[]]$DialogAnswers = @(), [string[]]$GuiKeys = @(), [string]$SavePath = "", [ValidateSet('list','grid')][string]$Chooser = 'list', [switch]$EnhancedMusic, [switch]$HotspotHints, [switch]$TouchControls, [ValidateRange(0,3600)][int]$AutosaveSeconds=0, [switch]$InventoryCompact, [switch]$ModernInventory, [switch]$RecoveryCompact)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force (Join-Path $root "build/test-saves") | Out-Null
if (!$SavePath) { $SavePath = Join-Path $root 'build/test-saves' }
$SavePath = [IO.Path]::GetFullPath($SavePath)
New-Item -ItemType Directory -Force $SavePath | Out-Null
$chooserConfig = if ($GuiKeys.Count) { "gui_saveload_chooser=$Chooser" } else { '' }
$subtitleConfig = switch ($SubtitleMode) {
    'on' { "subtitles=true`nspeech_mute=false" }
    'off' { "subtitles=false`nspeech_mute=false" }
    'only' { "subtitles=true`nspeech_mute=true" }
    default { '' }
}
$oldVideo = $env:SDL_VIDEODRIVER
$oldAudio = $env:SDL_AUDIODRIVER
try {
    $env:SDL_VIDEODRIVER = 'dummy'
    $env:SDL_AUDIODRIVER = 'dummy'
    $exe = if ($Executable) { [IO.Path]::GetFullPath($Executable) } else { Join-Path $root 'build/scummvm/Release/scummvm.exe' }
    foreach ($edition in $Editions) {
        $traceEvidence = $null
        $tracePassed = $false
        $nativeStarted = $false
        $process = $null
        $traceMutex = [Threading.Mutex]::new($false, "Local\DreamFactory-boottrace-$edition")
        $traceLocked = $false
        try {
        try { $traceLocked = $traceMutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $traceLocked = $true }
        if (!$traceLocked) { throw "$edition boot trace is already running; its config and log cannot be shared" }
        $game = if ($edition -eq 'dust') { 'dust' } else { 'taoot' }
        $data = Join-Path $root "game-data/$edition"
        $config = Join-Path $root "build/boot-$edition.ini"
        $checkpointPath = if ($Checkpoint) { $Checkpoint } else { Join-Path $root "build/checkpoint-$edition.dfsv" }
        $log = Join-Path $root "build/boot-$edition.log"
        $traceStarted = Get-Date
        $traceDirectory = Join-Path $root ("build/trace-history/" + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + "-$edition-" + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $traceDirectory | Out-Null
        $traceRestore = ''
        $traceEvidence = [ordered]@{
            schema=1; edition=$edition; status='failed'; started=$traceStarted.ToUniversalTime().ToString('o')
            binarySha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
            traceToolSha256=(Get-FileHash -LiteralPath $MyInvocation.MyCommand.Path -Algorithm SHA256).Hash
            originalRestore=$RestoreCheckpoint; restorePath=''; checkpointPath=$checkpointPath
            continuityEligible=(!$GuiKeys.Count -and $DialogSlot -lt 0 -and !@($Actions | Where-Object { $_.Trim() -match '^(loadcheckpoint|loadslot|rejectcheckpoint|clickon (button|prop) (load|restore)[^\s]*)(\s|$)' }).Count)
            artifacts=@{}
        }
        if ($RestoreCheckpoint) {
            # Load the archived bytes, not a mutable helper output from an earlier stage.
            $traceRestore = Join-Path $traceDirectory 'input.dfsv'
            Copy-Item -LiteralPath $RestoreCheckpoint -Destination $traceRestore
            $traceEvidence.restorePath = $traceRestore
            $traceEvidence.artifacts.input = @{ file='input.dfsv'; sha256=(Get-FileHash -LiteralPath $traceRestore -Algorithm SHA256).Hash }
        }
        $dialogConfig = if ($DialogSlot -ge 0) { "dreamfactory_boot_dialog_slot=$DialogSlot" } else { "" }
        $fontConfig = if ($DefaultFont) { "" } else { "dreamfactory_font=$root/build/runtime-data/LiberationSans-Regular.ttf" }
        $extra = if ($ExtraPath) { $ExtraPath } else { "$root/build/runtime-data" }
        @"
[scummvm]
enable_unsupported_game_warning=false
gui_theme=builtin
gfx_mode=surfacesdl
$subtitleConfig
joystick_num=-1
savepath=$SavePath
autosave_period=$AutosaveSeconds
$chooserConfig

[audit]
engineid=dreamfactory
gameid=$game
path=$data
language=en
platform=windows
extrapath=$extra
$fontConfig
dreamfactory_boot_trace=true
dreamfactory_enhanced_music=$($EnhancedMusic.ToString().ToLowerInvariant())
dreamfactory_hotspot_hints=$($HotspotHints.ToString().ToLowerInvariant())
dreamfactory_touch_controls=$($TouchControls.ToString().ToLowerInvariant())
dreamfactory_boot_checkpoint=$checkpointPath
$dialogConfig
dreamfactory_boot_restore=$traceRestore
dreamfactory_boot_steps=$Steps
dreamfactory_boot_choices=$($Choices -join ',')
dreamfactory_boot_optional_choices=$(if ($OptionalChoices.Count) { $OptionalChoices -join "," } else { $env:DREAMFACTORY_BOOT_OPTIONAL_CHOICES })
dreamfactory_boot_dialog_answers=$($DialogAnswers -join ',')
dreamfactory_boot_gui_keys=$($GuiKeys -join ',')
dreamfactory_inventory_test_compact=$($InventoryCompact.ToString().ToLowerInvariant())
dreamfactory_modern_inventory=$($ModernInventory.ToString().ToLowerInvariant())
dreamfactory_recovery_test_compact=$($RecoveryCompact.ToString().ToLowerInvariant())
dreamfactory_boot_actions=$($Actions -join ';')
dreamfactory_boot_keys=$($Keys -join ',')
dreamfactory_boot_click_movies=$($ClickMovies -join ',')
dreamfactory_boot_movie_clicks=$($MovieClicks -join ';')
dreamfactory_boot_modal_clicks=$($ModalClicks -join ';')
dreamfactory_boot_skip_movies=$($SkipMovies -join ',')
dreamfactory_boot_skip_speech=$($SkipSpeech.IsPresent.ToString().ToLowerInvariant())
dreamfactory_boot_click_x=$ClickX
dreamfactory_boot_click_y=$ClickY
dreamfactory_boot_capture=$root/build/boot-$edition.rgba
dreamfactory_boot_choose_game=$($ChooseGame.IsPresent.ToString().ToLowerInvariant())
"@ | Set-Content $config
        # ScummVM can rewrite its config at exit; preserve the launch input first.
        Copy-Item -LiteralPath $config -Destination "$traceDirectory/config.ini"
        $traceEvidence.artifacts.config = @{ file='config.ini'; sha256=(Get-FileHash -LiteralPath "$traceDirectory/config.ini" -Algorithm SHA256).Hash }
        $process = Start-Process -FilePath $exe -ArgumentList "--config=`"$config`"","--logfile=`"$log`"","--debuglevel=$DebugLevel",'audit' -WindowStyle Hidden -PassThru
        $nativeStarted = $true
        if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            throw "$edition boot trace timed out; inspect $log"
        }
        $process.Refresh()
        if ($process.ExitCode -ne 0) { throw "$edition native trace exited with code $($process.ExitCode); inspect $log" }
        $text = Get-Content $log -Raw
        $rows = $text -split "`r?`n" | Select-String 'BOOT (READY|STOP|PATH|CASTS|SHOPS|LOCATION|STEPS|KEY|CLICK|ACTION|TARGET|CHOICE)'
        if (!$rows) { throw "$edition produced no boot trace" }
        if ($text -match 'BOOT ACTION FAIL|BOOT STOP') { throw "$edition session action failed; inspect $log" }
        if ($text -notmatch 'BOOT READY') { throw "$edition did not complete its trace; inspect $log" }
        if ($text -notmatch 'BOOT CAPTURE PASS') { throw "$edition native startup capture failed; inspect $log" }
        if ($Steps -gt 0 -and $text -notmatch 'BOOT STEPS PASS') { throw "$edition post-startup session failed; inspect $log" }
        $tracePassed = $true
        Write-Output $edition
        $rows | ForEach-Object { $_.Line }
        } finally {
            try {
                if ($traceEvidence) {
                    $traceEvidence.status = if ($tracePassed) { 'passed' } else { 'failed' }
                    $traceEvidence['finished'] = (Get-Date).ToUniversalTime().ToString('o')
                    if ($nativeStarted -and (Test-Path -LiteralPath $log)) {
                        Copy-Item -LiteralPath $log -Destination "$traceDirectory/native.log"
                        $traceEvidence.artifacts.log = @{ file='native.log'; sha256=(Get-FileHash -LiteralPath "$traceDirectory/native.log" -Algorithm SHA256).Hash }
                        $nativeText = Get-Content "$traceDirectory/native.log" -Raw
                        if ($nativeText -match 'BOOT ACTION PASS .*?action=savecheckpoint(?: room=|!)' -and (Test-Path -LiteralPath $checkpointPath) -and (Get-Item -LiteralPath $checkpointPath).LastWriteTime -ge $traceStarted) {
                            Copy-Item -LiteralPath $checkpointPath -Destination "$traceDirectory/output.dfsv"
                            $traceEvidence.artifacts.output = @{ file='output.dfsv'; sha256=(Get-FileHash -LiteralPath "$traceDirectory/output.dfsv" -Algorithm SHA256).Hash }
                        }
                    }
                    $traceEvidence | ConvertTo-Json -Depth 7 | Set-Content "$traceDirectory/record.json"
                }
            } finally {
                if ($traceLocked) { $traceMutex.ReleaseMutex() }
                $traceMutex.Dispose()
            }
        }
    }
} finally {
    $env:SDL_VIDEODRIVER = $oldVideo
    $env:SDL_AUDIODRIVER = $oldAudio
}
