param([ValidateSet('demo','full','dust')][string]$Edition = 'demo', [switch]$Gameplay, [switch]$Viewer, [switch]$SoftwareRenderer, [switch]$EnhancedMusic,
    [ValidateSet('Smooth','Sharp')][string]$Presentation = 'Smooth', [switch]$PrepareOnly,
    [switch]$Accessible, [switch]$HotspotHints, [switch]$TouchControls)
$ErrorActionPreference = 'Stop'
if ($Gameplay -and $Viewer) { throw 'Choose gameplay or the developer viewer, not both' }
$root = Split-Path $PSScriptRoot -Parent
$game = if ($Edition -eq 'dust') { 'dust' } else { 'taoot' }
$config = Join-Path $root "build/play-$Edition.ini"
$data = Join-Path $root "game-data/$Edition"
$exe = Join-Path $root 'build/scummvm/Release/scummvm.exe'
$graphicsMode = if ($SoftwareRenderer) { 'surfacesdl' } else { 'opengl' }
$filtering = if ($Presentation -eq 'Smooth') { 'true' } else { 'false' }
$stretchMode = if ($Presentation -eq 'Smooth') { 'fit' } else { 'pixel-perfect' }
if (!(Test-Path $exe)) { throw 'Build the engine first with tools/build.ps1' }
@"
[scummvm]
enable_unsupported_game_warning=false
gui_theme=builtin
gfx_mode=$graphicsMode
filtering=$filtering
stretch_mode=$stretchMode
aspect_ratio=false
shader=default

[dreamfactory]
engineid=dreamfactory
gameid=$game
path=$data
language=en
platform=windows
extrapath=$root/build/runtime-data
dreamfactory_font=$root/build/runtime-data/LiberationSans-Regular.ttf
dreamfactory_viewer=$($Viewer.IsPresent.ToString().ToLowerInvariant())
dreamfactory_enhanced_music=$($EnhancedMusic.IsPresent.ToString().ToLowerInvariant())
dreamfactory_large_text=$($Accessible.IsPresent.ToString().ToLowerInvariant())
dreamfactory_contrast=$($Accessible.IsPresent.ToString().ToLowerInvariant())
dreamfactory_dialogue_controls=$($Accessible.IsPresent.ToString().ToLowerInvariant())
dreamfactory_hotspot_hints=$($HotspotHints.IsPresent.ToString().ToLowerInvariant())
dreamfactory_touch_controls=$($TouchControls.IsPresent.ToString().ToLowerInvariant())
"@ | Set-Content $config
if ($PrepareOnly) {
    Write-Output "Prepared $Presentation presentation config: $config"
    return
}
$previousRenderer = $env:SDL_RENDER_DRIVER
try {
    if ($SoftwareRenderer) { $env:SDL_RENDER_DRIVER = 'software' }
    Start-Process -FilePath $exe -ArgumentList "--config=`"$config`"",'dreamfactory' -WorkingDirectory $root -WindowStyle Normal
} finally { $env:SDL_RENDER_DRIVER = $previousRenderer }
