$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
$oldVideo = $env:SDL_VIDEODRIVER
$oldAudio = $env:SDL_AUDIODRIVER
try {
    $env:SDL_VIDEODRIVER = 'dummy'
    $env:SDL_AUDIODRIVER = 'dummy'
    New-Item -ItemType Directory -Force build/screenshots | Out-Null
    $exe = Join-Path $root 'build/scummvm/Release/scummvm.exe'
    foreach ($edition in 'demo','full','dust') {
        $game = if ($edition -eq 'dust') { 'dust' } else { 'taoot' }
        $data = Join-Path $root "game-data/$edition"
        $screenshots = Join-Path $root 'build/screenshots'
        $reference = & node tools/reference-audit.mjs $edition
        if ($LASTEXITCODE) { throw "Reference audit failed for $edition" }
        $reference | Set-Content "build/reference-$edition.log"
        foreach ($mode in 'verify','smoke') {
            $config = Join-Path $root "build/$mode-$edition.ini"
            $log = Join-Path $root "build/$mode-$edition.log"
            @"
[scummvm]
enable_unsupported_game_warning=false
gui_theme=builtin
gfx_mode=surfacesdl
screenshotpath=$screenshots

[audit]
engineid=dreamfactory
gameid=$game
path=$data
language=en
platform=windows
dreamfactory_$mode=true
"@ | Set-Content $config
            $process = Start-Process -FilePath $exe -ArgumentList "--config=`"$config`"","--logfile=`"$log`"",'audit' -WindowStyle Hidden -PassThru
            if (!$process.WaitForExit(60000)) {
                Stop-Process -Id $process.Id -Force
                throw "$edition $mode timed out; inspect $log"
            }
            $text = Get-Content $log -Raw
            if ($text -notmatch "$($mode.ToUpper()) PASS") { throw "$edition $mode failed; inspect $log" }
            if ($mode -eq 'verify') {
                if ($text -notmatch 'DEPTH CODEC PASS') { throw "$edition depth codec checks failed" }
                $nativeRows = [regex]::Matches($text, '(?:VERIFY|DEPTH) \S+\.SET frames=\d+ fnv1a=[0-9a-f]+') | ForEach-Object { $_.Value }
                $referenceRows = $reference | Where-Object { $_ -match '^(?:VERIFY|DEPTH) \S+\.SET ' }
                $difference = Compare-Object $nativeRows $referenceRows
                if ($difference) { $difference | Out-String | Write-Output; throw "$edition pixel hashes differ" }
            }
            Write-Output "$edition $mode PASS"
        }
    }
} finally {
    $env:SDL_VIDEODRIVER = $oldVideo
    $env:SDL_AUDIODRIVER = $oldAudio
    Pop-Location
}
