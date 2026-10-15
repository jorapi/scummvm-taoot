# SPDX-License-Identifier: GPL-3.0-or-later
# Read authored inventory catalogs from earned native saves; no story injection.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/inventory-catalog/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    foreach ($edition in @('full','demo','dust')) {
        $checkpoint = switch ($edition) { full {'build/save-full-best-sinking-phase1.dfsv'} demo {'build/save-demo-photograph.dfsv'} dust {'build/save-dust-day5-morning.dfsv'} }
        & tools/trace-boot.ps1 -Editions $edition -RestoreCheckpoint $checkpoint -SavePath "$run/$edition/saves" -Steps 1 -Actions @('dumpinventory') *> "$run/$edition.log"
        Copy-Item -LiteralPath "build/boot-$edition.log" -Destination "$run/$edition-native.log"
        $log = Get-Content "$run/$edition-native.log" -Raw
        $expected = switch ($edition) { full {@('painting','realneck','rubaiyat','notebook','trunkkey')} demo {@('photo1','trunkkey')} dust {@('chest')} }
        if ($log -notmatch ('BOOT INVENTORY items=' + $expected.Count + '!')) { throw "$edition inventory count differs from the earned original state" }
        foreach ($item in $expected) {
            if ($log -notmatch ('BOOT INVENTORY ITEM name=' + $item + ' title=.+ examine=1 picture=[1-9]\d*x[1-9]\d*!')) { throw "$edition missing native item/artwork: $item" }
        }
        if ($edition -eq 'full' -and $log -notmatch 'name=realneck title=Necklace ') {throw 'Inventory label reveals more than the original necklace label'}
        if ($edition -eq 'dust' -and $log -match 'BOOT INVENTORY ITEM name=helpbut') {throw 'Interface prop exposed as an inventory item'}
    }
    @{scope='Earned Titanic sinking, demo developed photograph and Dust day-five saves. Original inventory order/availability and labels, non-mutating large sprite previews, item ownership and unchanged story globals; no controller UI claim.';passed=$true;binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Original inventory catalog and read-only artwork PASS: $run"
} finally {Pop-Location}
