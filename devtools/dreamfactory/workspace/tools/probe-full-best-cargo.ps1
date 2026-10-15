$root = Get-Location
function Route([string]$room,[string]$from,[string]$to) {
 $r = & node tools/room-route.mjs full $room $from $to
 if ($LASTEXITCODE) { throw 'Route failure' }
 $r | ForEach-Object { ($_ -split ' # ')[0] }
}
& tools/trace-boot.ps1 -Editions full -RestoreCheckpoint build/save-full-cargo-star.dfsv -Checkpoint build/save-full-best-cargo-probe.dfsv -Steps 1 -TimeoutSeconds 120 -MovieClicks @('lightoff.mov:switch.SE','lightoff.mov:#1') -Actions (@('expectglobal hallside star') + @(Route CARGO View18 View16) + @('clickon painting door','key uparrow','expectset bind','expectview View15') + @(Route BIND View15 View16) + @('clickon painting car','expectglobal carlights 1','expectset binl','expectview View16') + @(Route BINL View16 View17) + @('clickon painting crate','expectflat cargo 1','dumpprop cargopainting','expectowner painting none','targets','savecheckpoint'))
Copy-Item build/boot-full.log build/full-best-cargo-probe-native.log
Copy-Item build/boot-full.rgba build/full-best-cargo-probe.rgba
python tools/preview-rgba.py build/full-best-cargo-probe.rgba
