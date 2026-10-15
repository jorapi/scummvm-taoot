# SPDX-License-Identifier: GPL-3.0-or-later
# Exercise native inventory scripts and mapped controller input from earned saves.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/inventory-ui/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    function Test-InventoryCase([string]$Name, [hashtable]$Parameters) {
        $Parameters.SavePath = "$run/$Name/saves"
        $Parameters.Checkpoint = "$run/$Name.dfsv"
        $Parameters.SkipSpeech = $true
        $Parameters.TimeoutSeconds = 240
        & tools/trace-boot.ps1 @Parameters *> "$run/$Name.log"
        Copy-Item -LiteralPath "build/boot-$($Parameters.Editions).log" -Destination "$run/$Name-native.log"
        Write-Output "PASS $Name"
    }
    foreach ($compact in @($false, $true)) {
        $size = if ($compact) {'compact'} else {'desktop'}
        foreach ($edition in @('full', 'demo', 'dust')) {
            $save = switch ($edition) {full {'save-full-best-sinking-phase1'} demo {'save-demo-photograph'} dust {'save-dust-day5-morning'}}
            $item = switch ($edition) {full {'painting'} demo {'photo1'} dust {'chest'}}
            $movie = switch ($edition) {full {'painting.mov'} demo {'iphoto1g.mov','iphoto1b.mov'} dust {'chest.mov'}}
            $owner = if ($edition -eq 'dust') {'stranger'} else {'frank'}
            Test-InventoryCase "$edition-examine-$size" @{Editions=$edition; RestoreCheckpoint="build/$save.dfsv"; InventoryCompact=$compact; GuiKeys=@('home','pad:y','snapshot','pad:b'); SkipMovies=@($movie); Actions=@('openinventory',"expectowner $item $owner", "expectglobal handitem $item")}
        }
    }
    Test-InventoryCase 'dust-scroll' @{Editions='dust'; RestoreCheckpoint='build/save-dust-day3-afternoon-ready.dfsv'; InventoryCompact=$true; GuiKeys=@('home','snapshot','point:50:78','wheel:1','snapshot','drag:50:180:70','snapshot','end','snapshot','pad:a'); Actions=@('openinventory','expectglobal handitem badge','expectowner badge stranger')}
    Test-InventoryCase 'demo-trunk' @{Editions='demo'; RestoreCheckpoint='build/save-demo-opening.dfsv'; InventoryCompact=$true; GuiKeys=@('snapshot','pad:a'); Actions=@('openinventory','expectglobal handitem trunkkey','key rightarrow','key rightarrow','key rightarrow','key rightarrow','key uparrow','key leftarrow','expectview View66','padon painting trunk','expectflat trunk 1','expectowner trunkkey frank')}
    Test-InventoryCase 'dust-give-ring' @{Editions='dust'; RestoreCheckpoint='build/save-dust-blood-hotel.dfsv'; InventoryCompact=$true; GuiKeys=@('pad:up','snapshot','pad:a'); Choices=@('fear.pup:101'); Actions=@('expectowner ring stranger','expectglobal handitem cigar','openinventory','expectglobal handitem ring','padon actor fear','expectowner ring fear','expectglobal phase 5','expectglobal playercash 6')}
    Test-InventoryCase 'full-conversation-gaspen' @{Editions='full'; RestoreCheckpoint='build/save-full-notebook-top.dfsv'; InventoryCompact=$true; ModernInventory=$true; GuiKeys=@('snapshot','pad:up','snapshot','pad:a'); Choices=@('zeit1.pup:101','zeit1.pup:101','zeit1.pup:101','zeit1.pup:101','zeit1.pup:55555','zeit1.pup:55555'); ClickMovies=@('berg.mov'); Actions=@('expectset smstack3','expectview View53','expectglobal mission 3','expectglobal phase 2','clickon prop notebook','expectglobal mission 3','expectglobal phase 3','expectowner notebook frank','expectowner gaspen zeit','expectglobal lockevents 0')}
    @{passed=$true; binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash; scope='Native desktop/480x272 inventory, mapped controller examine/back/use/give, pointer/wheel/drag scrolling and genuine object/conversation scripts from earned saves. Physical handhelds remain untested.'} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Inventory UI PASS: $run"
} finally {Pop-Location}
