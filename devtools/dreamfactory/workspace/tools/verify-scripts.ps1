param([switch]$Puppets, [switch]$Screens, [switch]$Cursors, [switch]$Rooms, [switch]$Stars, [switch]$Stages, [switch]$Resources, [switch]$Audio, [switch]$Casts, [switch]$Movies, [switch]$Shops)
$ErrorActionPreference = 'Stop'
$auditKind = if ($Puppets) { 'puppet' } elseif ($Screens) { 'screen' } elseif ($Cursors) { 'cursor' } elseif ($Rooms) { 'room' } elseif ($Stars) { 'star' } elseif ($Shops) { 'shop' } elseif ($Movies) { 'movie' } elseif ($Casts) { 'cast' } elseif ($Audio) { 'audio' } elseif ($Resources) { 'resource' } elseif ($Stages) { 'stage' } else { 'script' }
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
$oldVideo = $env:SDL_VIDEODRIVER
$oldAudio = $env:SDL_AUDIODRIVER
try {
    $env:SDL_VIDEODRIVER = 'dummy'
    $env:SDL_AUDIODRIVER = 'dummy'
    if ($Puppets) { & node tools/movie-fixtures.mjs; if ($LASTEXITCODE) { throw 'Movie fixture generation failed' }; & node tools/puppet-fixtures.mjs; if ($LASTEXITCODE) { throw 'Puppet fixture generation failed' } }
    if ($Screens) { & node tools/screen-fixtures.mjs; if ($LASTEXITCODE) { throw 'Screen fixture generation failed' } }
    if ($Stages) { & node tools/stage-fixtures.mjs; if ($LASTEXITCODE) { throw 'Stage fixture generation failed' } }
    if ($Movies) { & node tools/movie-fixtures.mjs; if ($LASTEXITCODE) { throw 'Movie fixture generation failed' } }
    if ($Shops) { & node tools/prop-fixtures.mjs; if ($LASTEXITCODE) { throw 'Prop fixture generation failed' } }
    if ($Audio) { & node tools/ambient-fixtures.mjs; if ($LASTEXITCODE) { throw 'Ambient fixture generation failed' } }
    if ($Rooms) { & node tools/room-fixtures.mjs; if ($LASTEXITCODE) { throw 'Room fixture generation failed' } }
    $exe = Join-Path $root 'build/scummvm/Release/scummvm.exe'
    foreach ($edition in 'demo','full','dust') {
        $reference = & node "tools/$auditKind-audit.mjs" $edition
        if ($LASTEXITCODE) { throw "Reference $auditKind audit failed: $edition" }
        $reference | Set-Content "build/$auditKind-reference-$edition.log"
        $game = if ($edition -eq 'dust') { 'dust' } else { 'taoot' }
        $data = Join-Path $root "game-data/$edition"
        $fixtures = Join-Path $root 'build/script-tests'
        $config = Join-Path $root "build/$auditKind-$edition.ini"
        $log = Join-Path $root "build/$auditKind-$edition.log"
        @"
[scummvm]
enable_unsupported_game_warning=false
gui_theme=builtin
gfx_mode=surfacesdl
# Synthetic input must not mix with a connected physical controller.
joystick_num=-1

[audit]
engineid=dreamfactory
gameid=$game
path=$data
language=en
platform=windows
extrapath=$root/build/runtime-data
dreamfactory_font=$root/build/runtime-data/LiberationSans-Regular.ttf
dreamfactory_$($auditKind)_audit=true
dreamfactory_script_fixtures=$fixtures
dreamfactory_puppet_fixtures=$root/build/puppet-session-$edition
dreamfactory_cursor_fixtures=$root/build/cursor-tests-$edition.dfscript
dreamfactory_screen_fixtures=$root/build/screen-session-$edition
dreamfactory_resource_fixtures=$root/build/resource-tests-$edition.bin
dreamfactory_audio_fixtures=$root/build/audio-tests-$edition.dfscript
dreamfactory_cricket_fixtures=$root/build/ambient-session-$edition
dreamfactory_cricket_retail=$(($edition -eq 'full').ToString().ToLowerInvariant())
dreamfactory_cricket_demo=$(($edition -eq 'demo').ToString().ToLowerInvariant())
dreamfactory_room_fixtures=$root/build/room-session-$edition
dreamfactory_actor_fixtures=$root/build/cast-session-$edition
dreamfactory_movie_fixtures=$root/build/movie-session-$edition
dreamfactory_movie_retail=$(($edition -eq 'full').ToString().ToLowerInvariant())
dreamfactory_prop_fixtures=$root/build/prop-session-$edition
dreamfactory_stage_fixtures=$root/build/stage-session-$edition
"@ | Set-Content $config
        $process = Start-Process -FilePath $exe -ArgumentList "--config=`"$config`"","--logfile=`"$log`"",'audit' -WindowStyle Hidden -PassThru
        if (!$process.WaitForExit(60000)) {
            Stop-Process -Id $process.Id -Force
            throw "$edition $auditKind audit timed out; inspect $log"
        }
        $text = Get-Content $log -Raw
        if ($text -notmatch "$($auditKind.ToUpper()) PASS") { throw "$edition $auditKind audit failed; inspect $log" }
        if ($Stars -and $text -notmatch 'STAR READER PASS') { throw "$edition star reader checks failed; inspect $log" }
        if ($Puppets -and $text -notmatch 'JOURNAL POINTER PASS') { throw "$edition journal pointer selection/scroll/drag checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'HOVER CURSOR PASS') { throw "$edition hover cursor checks failed; inspect $log" }
        if ($Puppets -and $text -notmatch 'JOURNAL PASS') { throw "$edition journal capture/replay/modal UI checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'ROUTE GRAPH PASS') { throw "$edition route graph/codec checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'ROUTE MAP PASS') { throw "$edition route map layout checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'ROUTE ENDINGS PASS') { throw "$edition authored ending classification checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'ROUTE CATALOG PASS') { throw "$edition reached/unreached ending catalog checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'ROUTE INVENTORY PASS') { throw "$edition route inventory/object restore checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'ROUTE PREVIEW PASS') { throw "$edition checkpoint preview checks failed; inspect $log" }
        if ($Movies -and $text -notmatch 'ROUTE ENDING HOST PASS') { throw "$edition completed-movie route recording checks failed; inspect $log" }
        if ($Puppets -and $text -notmatch 'ROUTE ENCOUNTER PASS') { throw "$edition encounter checkpoint inheritance checks failed; inspect $log" }
        if ($Puppets -and $text -notmatch 'ROUTE UI PASS') { throw "$edition route map/controller/restore UI checks failed; inspect $log" }
        if ($Puppets -and $text -notmatch 'VLR CHART PASS') { throw "$edition chart overview/current/zoom checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'ROUTE STORE PASS') { throw "$edition route persistence checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'ROUTE SAVE PASS') { throw "$edition route save checks failed; inspect $log" }
        if ($Puppets -and ($text -notmatch 'ROUTE CAPTURE PASS' -or $text -notmatch 'ROUTE CHECKPOINT PASS')) { throw "$edition route capture checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'JOURNAL SAVE PASS') { throw "$edition journal save checks failed; inspect $log" }
        if ($Puppets -and $text -notmatch 'GAMEPAD DIALOGUE PASS') { throw "$edition gamepad dialogue checks failed; inspect $log" }
        if ($Stages -and $text -notmatch 'INPUT CONTROLS PASS') { throw "$edition touch/gamepad checks failed; inspect $log" }
        if ($Puppets -and ($text -notmatch 'DIALOGUE CONTROLS HOST PASS' -or $text -notmatch 'DIALOGUE TOUCH PASS' -or $text -notmatch 'DIALOGUE ACCESSIBILITY PASS')) { throw "$edition dialogue enhancement checks failed; inspect $log" }
        if ($Stages -and ($text -notmatch 'HOTSPOT HINTS PASS' -or $text -notmatch 'AUTOSAVE GUARD PASS')) { throw "$edition hotspot/autosave checks failed; inspect $log" }
        if ($Audio -and ($text -notmatch 'ARTWORK PACK PASS' -or $text -notmatch 'ENHANCEMENT MUSIC PASS')) { throw "$edition optional pack checks failed; inspect $log" }
        if ($Puppets -and ($text -notmatch 'PUPPET READER PASS' -or $text -notmatch 'PUPPET SESSION PASS' -or $text -notmatch 'PUPPET SPEECH STATE PASS' -or $text -notmatch 'PUPPET SPEECH HOST PASS' -or $text -notmatch 'PUPPET ART FIXTURE PASS' -or $text -notmatch 'PUPPET CHOICE STATE PASS' -or $text -notmatch 'PUPPET CHOICE HOST PASS' -or $text -notmatch 'PUPPET TEXT PASS' -or $text -notmatch 'PUPPET DISPLAY PASS' -or $text -notmatch 'PUPPET AUTHORED HOST PASS')) { throw "$edition puppet reader/session checks failed; inspect $log" }
        if ($Cursors -and $text -notmatch 'CURSOR API PASS') { throw "$edition cursor script checks failed; inspect $log" }
        if ($Screens -and ($text -notmatch 'SCREEN PALETTE PASS' -or $text -notmatch 'SCREEN TIMING PASS' -or $text -notmatch 'SCREEN TRANSITION PASS' -or $text -notmatch 'SCREEN QUIT PASS')) { throw "$edition palette/fade checks failed; inspect $log" }
        if ($Rooms -and ($text -notmatch 'ROOM CAMERA PASS' -or $text -notmatch 'ROOM READER PASS' -or $text -notmatch 'ROOM SESSION PASS' -or $text -notmatch 'ROOM SESSION BINDING PASS' -or $text -notmatch 'ROOM DISPLAY PASS')) { throw "$edition room script/display checks failed; inspect $log" }
        if ($Stages -and ($text -notmatch 'STAGE TEXT PASS' -or $text -notmatch 'STAGE SESSION PASS' -or $text -notmatch 'STAGE SESSION REGIONS PASS' -or $text -notmatch 'STAGE ACTOR DISPLAY PASS' -or $text -notmatch 'ROOM SPRITE DISPLAY PASS' -or $text -notmatch 'INPUT PASS' -or $text -notmatch 'INPUT HIT PASS' -or $text -notmatch 'INPUT ANGLES PASS' -or $text -notmatch 'STAGE WORLD DISPLAY PASS' -or $text -notmatch 'STAGE DISPLAY PASS')) { throw "$edition stage lifecycle checks failed; inspect $log" }
        if ($Audio -and $text -notmatch 'SOUND PASS') { throw "$edition mixer playback smoke failed; inspect $log" }
        if ($Audio -and $text -match 'out of mixer slots') { throw "$edition exhausted mixer channels; inspect $log" }
        if ($Audio -and $edition -eq 'dust' -and $text -notmatch 'BALL PASS') { throw "$edition projectile checks failed; inspect $log" }
        if ($Audio -and $edition -eq 'dust' -and $text -notmatch 'DUST DIRECTORY PASS') { throw "$edition one-shot/music directory and legacy save checks failed; inspect $log" }
        if ($Audio -and $edition -ne 'dust' -and ($text -notmatch 'CRICKET PASS' -or $text -notmatch 'CRICKET MIXER PASS')) { throw "$edition ambient checks failed; inspect $log" }
        if ($Audio -and $edition -eq 'full' -and $text -notmatch 'CRICKET PRIORITY PASS') { throw "Retail ambient priority checks failed; inspect $log" }
        if ($Audio -and $edition -eq 'full' -and $text -notmatch 'CRICKET SLOT PASS') { throw "Retail ambient slot/save checks failed; inspect $log" }
        if ($Audio -and $edition -ne 'full' -and $text -notmatch 'EARLY SOUND PASS') { throw "$edition physical slot/bank/save checks failed; inspect $log" }
        if ($Audio -and $text -notmatch 'BANK CONTROLS PASS') { throw "$edition per-bank sound control checks failed; inspect $log" }
        if ($Audio -and $text -notmatch 'BANK OWNERS PASS') { throw "$edition duplicate bank/theme control/owner save checks failed; inspect $log" }
        if ($Audio -and ($text -notmatch 'LIVE CONTROLS PASS' -or $text -notmatch 'LOOP STREAM PASS')) { throw "$edition live sound control checks failed; inspect $log" }
        if ($Audio -and $text -notmatch 'PCM OWNERSHIP PASS') { throw "$edition PCM lifetime checks failed; inspect $log" }
        if ($Audio -and ($text -notmatch 'THEME STREAM PASS' -or $text -notmatch 'THEME QUEUE PASS')) { throw "$edition theme queue/PCM/save checks failed; inspect $log" }
        if ($Casts -and $text -notmatch 'ACTOR DIST PASS') { throw "$edition actor distance/order checks failed; inspect $log" }
        if ($Casts -and $text -notmatch 'ACTOR RENDER PASS') { throw "$edition actor rendering checks failed; inspect $log" }
        if ($Casts -and $text -notmatch 'ACTOR PASS') { throw "$edition cast lifecycle smoke failed; inspect $log" }
        if ($Casts -and $edition -eq 'dust' -and $text -notmatch 'HITBOX PASS') { throw 'Dust hit box checks failed' }
        if ($Casts -and $text -notmatch 'STAR SESSION PASS') { throw "$edition star placement checks failed; inspect $log" }
        if ($Casts -and $text -notmatch 'MOTION PASS') { throw "$edition actor motion checks failed; inspect $log" }
        if ($Casts -and $text -notmatch 'TIMING PASS') { throw "$edition real-clock pacing smoke failed; inspect $log" }
        if ($Movies -and ($text -notmatch 'MOVIE STATE PASS' -or $text -notmatch 'MOVIE READER PASS')) { throw "$edition movie state/reader checks failed; inspect $log" }
        if ($Movies -and ($text -notmatch 'MOVIE HINTS PASS' -or $text -notmatch 'MOVIE HINT HOST PASS')) { throw "$edition movie hotspot hint checks failed; inspect $log" }
        if ($Movies -and $text -notmatch 'GAMEPAD MOVIE POINTER PASS') { throw "$edition close-up controller pointer checks failed; inspect $log" }
        if ($Movies -and $text -notmatch 'GAMEPAD MOVIE PASS') { throw "$edition gamepad movie checks failed; inspect $log" }
        if ($Movies -and $text -notmatch 'MOVIE HOST PASS') { throw "$edition modal movie host checks failed; inspect $log" }
        if ($Movies -and $text -notmatch 'MOVIE VOICE PASS') { throw "$edition shared movie voice checks failed; inspect $log" }
        if ($Movies -and $text -notmatch 'MOVIE ORDER PASS') { throw "$edition authored movie soundtrack checks failed; inspect $log" }
        if ($Movies -and $text -notmatch 'MOVIE THEME PASS') { throw "$edition shared movie theme checks failed; inspect $log" }
        if ($Movies -and $text -notmatch ('MOVIE VOICE PASS retail=' + [int]($edition -eq 'full'))) { throw "$edition movie fixture edition policy mismatch; inspect $log" }
        if ($Shops -and $text -notmatch 'SHOP READER PASS') { throw "$edition shop reader checks failed; inspect $log" }
        if ($Shops -and $text -notmatch 'PROP WORLD PASS') { throw "$edition world prop checks failed; inspect $log" }
        if ($Shops -and $text -notmatch 'PROP PASS') { throw "$edition prop lifecycle checks failed; inspect $log" }
        if ($Shops -and $text -notmatch 'PROP VISIBILITY PASS') { throw "$edition prop visibility checks failed; inspect $log" }
        if ($Shops -and ($text -notmatch 'PROP STAR PASS' -or $text -notmatch 'PROP RENDER PASS')) { throw "$edition prop star placement checks failed; inspect $log" }
        $pattern = '(?:SCRIPT \S+ scripts=\d+ handlers=\d+ statements=\d+|TEST \S+ value=.*? string=\d consumed=\d)'
        if ($Puppets) { $pattern = '(?:PUPPETART \S+ pictures=\d+ rgba=[0-9a-f]+|PUPPET \S+ lines=\d+ scripts=\d+ stances=\d+ frames=\d+ poses=\d+ samples=\d+ meta=[0-9a-f]+ pixels=[0-9a-f]+ audio=[0-9a-f]+)' }
        if ($Screens) { $pattern = 'SCREEN PASS' }
        if ($Cursors) { $pattern = 'CURSOR \S+ width=\d+ height=\d+ x=\d+ y=\d+ rgba=[0-9a-f]+' }
        if ($Stars) { $pattern = 'STAR \S+ stars=\d+ paths=\d+ points=\d+ meta=[0-9a-f]+' }
        if ($Rooms) { $pattern = '(?:ROOMDEPTH \S+ images=\d+ planes=\d+ fnv1a=[0-9a-f]+|ROOMCAM \S+ frames=\d+ meta=[0-9a-f]+|ROOM \S+ scenes=\d+ paintings=\d+ meta=[0-9a-f]+)' }
        if ($Stages) { $pattern = '(?:STAGEART \S+ pictures=\d+ rgba=[0-9a-f]+|STAGE \S+ flats=\d+ regions=\d+ frames=\d+ meta=[0-9a-f]+ pixels=[0-9a-f]+)' }
        if ($Resources) { $pattern = 'RESOURCE \S+ cd=\S+ path=\S+ size=\d+ fnv1a=[0-9a-f]+' }
        if ($Audio) { $pattern = 'AUDIO \S+ loops=\d+ sounds=\d+ chunks=\d+ samples=\d+ meta=[0-9a-f]+ pcm=[0-9a-f]+ theme=[0-9a-f]+' }
        if ($Casts) { $pattern = '(?:ACTORART \S+ pictures=\d+ selection=[0-9a-f]+ rgba=[0-9a-f]+|CAST \S+ members=\d+ poses=\d+ frames=\d+ meta=[0-9a-f]+ pixels=[0-9a-f]+)' }
        if ($Movies) { $pattern = 'MOVIE \S+ segments=\d+ frames=\d+ regions=\d+ chunks=\d+ meta=[0-9a-f]+ pixels=[0-9a-f]+ audio=[0-9a-f]+' }
        if ($Shops) { $pattern = '(?:GEOMETRY projection=[0-9a-f]+ bearing=[0-9a-f]+|PROPWORLD \S+ pictures=\d+ rgba=[0-9a-f]+|PROPART \S+ pictures=\d+ selection=[0-9a-f]+ rgba=[0-9a-f]+|SHOP \S+ props=\d+ states=\d+ frames=\d+ meta=[0-9a-f]+ pixels=[0-9a-f]+)' }
        $nativeRows = [regex]::Matches($text, $pattern) | ForEach-Object { $_.Value }
        # Deliberately stricter than the reference's unknown-call zero fallback.
        $referenceRows = $reference | Where-Object { $_ -match "^$pattern`$" } | ForEach-Object {
            if ($_ -match '^TEST missingcall.dfscript ') { 'TEST missingcall.dfscript value=ERROR string=0 consumed=0' } else { $_ }
        }
        $difference = Compare-Object $nativeRows $referenceRows
        if ($difference) { $difference | Out-String | Write-Output; throw "$edition $auditKind reference results differ" }
        Write-Output "$edition $auditKind PASS"
    }
} finally {
    $env:SDL_VIDEODRIVER = $oldVideo
    $env:SDL_AUDIODRIVER = $oldAudio
    Pop-Location
}
