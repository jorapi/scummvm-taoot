# SPDX-License-Identifier: GPL-3.0-or-later
param([switch]$CollectOnly, [string]$SnapshotsDirectory = 'build/navigation-earned')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/navigation-proof/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    $snapshots = [IO.Path]::GetFullPath($SnapshotsDirectory)
    New-Item -ItemType Directory -Force $run,$snapshots | Out-Null
    if (!$CollectOnly) {
        & tools/verify-scripts.ps1 -Stages *> "$run/stages.log"
        & tools/verify-scripts.ps1 -Puppets *> "$run/puppets.log"
        & tools/verify-full-gym.ps1 -ExtraActions @('dumpnavigation','savecheckpoint') *> "$run/full-journey.log"
        Copy-Item build/checkpoint-full.dfsv "$snapshots/full-briefing.dfsv"
        & tools/verify-demo-mission.ps1 -ExtraActions @('dumpnavigation','savecheckpoint') *> "$run/demo-journey.log"
        Copy-Item build/checkpoint-demo.dfsv "$snapshots/demo-briefing.dfsv"
    }
    $binary = Get-Item build/scummvm/Release/scummvm.exe
    foreach ($edition in @('full','demo','dust')) {
        foreach ($kind in @('stage','puppet')) {
            $log = Get-Item "build/$kind-$edition.log"
            if ($log.LastWriteTimeUtc -lt $binary.LastWriteTimeUtc) {throw "$edition $kind evidence predates the current executable"}
            Copy-Item $log.FullName "$run/$edition-$kind.log"
        }
        if ((Get-Content "$run/$edition-stage.log" -Raw) -notmatch 'NAVIGATION SAVE PASS.*exact encounter positions.*recovery round trip') {throw "$edition lacks native navigation save/recovery evidence"}
        if ((Get-Content "$run/$edition-puppet.log" -Raw) -notmatch 'NAVIGATION HISTORY PASS') {throw "$edition lacks bounded directed-history evidence"}
    }
    function Plan($room,$from,$to) {
        $route = @(& node tools/room-route.mjs full $room $from $to | ForEach-Object {($_ -split ' # ')[0]})
        if ($LASTEXITCODE) {throw "Cannot plan $room navigation"}
        return $route
    }
    function Trace($edition,$restore,$label,$actions,$keys=@(),[switch]$NoHistory) {
        & tools/trace-boot.ps1 -Editions $edition -RestoreCheckpoint $restore -Checkpoint "$run/$label.dfsv" -SavePath "$run/saves" -GuiKeys $keys -Actions $actions -NoNavigationHistory:$NoHistory *> "$run/$label.log"
        Copy-Item "build/boot-$edition.log" "$run/$label-native.log"
        return Get-Content "$run/$label-native.log" -Raw
    }
    function Counts($text) {
        $matches = [regex]::Matches($text, 'BOOT NAVIGATION .*visits=(\d+) passages=(\d+) valid=1')
        if (!$matches.Count) {throw 'No valid native map position in trace'}
        $last = $matches[$matches.Count - 1]
        return @{visits=[int]$last.Groups[1].Value; passages=[int]$last.Groups[2].Value}
    }
    $gui = @('snapshot','pad:y','snapshot','pad:a','snapshot','pad:b','pad:b')
    foreach ($edition in @('full','demo')) {
        $checkpoint = "$snapshots/$edition-briefing.dfsv"
        if (!(Test-Path $checkpoint)) {throw "Missing earned $edition first-briefing snapshot"}
        Copy-Item $checkpoint "$run/$edition-briefing.dfsv"
        foreach ($size in @('desktop','compact')) {
            $command = if ($size -eq 'compact') {'openmapcompact'} else {'openmap'}
            $text = Trace $edition $checkpoint "$edition-$size" @($command,'auditmapinput','dumpnavigation') $gui
            if ($text -notmatch 'NAVIGATION KEYMAP PASS' -or $text -notmatch 'NAVIGATION INPUT PASS.*snapshot unchanged') {throw "$edition $size lacks input/state isolation proof"}
            $position = if ($edition -eq 'full') {'room=gym .*page=1 x=327 y=185'} else {'room=gstair1 .*page=2 x=94 y=74'}
            if ($text -notmatch "BOOT NAVIGATION $position") {throw "$edition authored map placement changed"}
            if ($text -notmatch 'render_paragraph\(Penny Pringle\)') {throw "$edition lacks its actually heard encounter label"}
            $captures = @(Get-ChildItem "$run/$edition-$size.dfsv.gui-*.bmp")
            if ($captures.Count -lt 3) {throw 'Map fit, zoom and reader captures are missing'}
            foreach ($capture in $captures) {
                $header = [IO.File]::ReadAllBytes($capture.FullName)
                if ($size -eq 'compact' -and ([BitConverter]::ToInt32($header,18) -ne 480 -or [Math]::Abs([BitConverter]::ToInt32($header,22)) -ne 272)) {throw 'Compact capture is not a native 480x272 overlay'}
            }
        }
    }
    $walk = @('expectset gym','expectview View47') + @(Plan GYM View47 View45) + @('clickon painting door','key uparrow','expectset veststbd','expectview View20') + @(Plan VESTSTBD View20 View21) + @('clickon painting door','key uparrow','expectset gym','expectview View47','openmapcompact','savecheckpoint','dumpnavigation')
    $text = Trace full "$snapshots/full-briefing.dfsv" 'walked-return' $walk @('snapshot','pad:up','pad:x','snapshot','pad:b','pad:y','drag:240:160:110','snapshot','pad:b')
    if ($text -notmatch 'render_paragraph\(Turn right' -or $text -notmatch 'BOOT GUI POINTER drag:') {throw 'Authored return-route direction or touch panning is missing'}
    $before = Counts $text
    $text = Trace full "$run/walked-return.dfsv" 'original-map-jump' @('dumpnavigation','clickon prop map','key e','expectflat Map 6','clickon button Button43','expectset turkstrs','dumpnavigation','savecheckpoint')
    $jumped = Counts $text
    if ($jumped.visits -le $before.visits -or $jumped.passages -ne $before.passages) {throw 'Original map jump did not preserve the walking graph'}
    $text = Trace full "$run/walked-return.dfsv" 'rewound' @('dumpnavigation','auditmapinput')
    $rewound = Counts $text
    if ($rewound.visits -ne $before.visits -or $rewound.passages -ne $before.passages) {throw 'Later map visits leaked into the restored snapshot'}
    & tools/trace-boot.ps1 -Editions full -ChooseGame -SkipMovies @('logo.mov','datebed.mov') -Steps 40 -TimeoutSeconds 180 -NoNavigationHistory -Checkpoint "$run/recording-off.dfsv" -SavePath "$run/saves" -Actions @('expectset bedsit1','key rightarrow','key leftarrow','dumpnavigation','savecheckpoint') *> "$run/recording-off.log"
    Copy-Item build/boot-full.log "$run/recording-off-native.log"
    $off = Counts (Get-Content "$run/recording-off-native.log" -Raw)
    $disabledSave = [IO.File]::ReadAllBytes("$run/recording-off.dfsv")
    if ($off.visits -or $off.passages -or [BitConverter]::ToUInt32($disabledSave,4) -ge 12) {throw 'Disabled recording wrote new map history'}
    @{passed=$true; binarySha256=(Get-FileHash $binary.FullName -Algorithm SHA256).Hash; restoredVisits=$rewound.visits; restoredPassages=$rewound.passages; physicalDeviceTested=$false; goalComplete=$false; scope='Original retail/demo maps and actual spoken encounters; native desktop/480x272 fit, zoom, read, marker selection, wheel and touch pan, physical F11 and mapped controller input, byte-identical game state after browsing, actual walked next-turn guidance, original map teleport exclusion, native rewind, opt-out, legacy saves and v12 recovery. Dust has no ship-map UI; native save/control regressions are included.'} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Annotated navigation proof PASS: $run"
} finally {Pop-Location}
