# SPDX-License-Identifier: GPL-3.0-or-later
param([string]$RestoreCheckpoint = 'build/save-full-best-sinking-phase1.dfsv')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/route-preview-full/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    $inventory = @('expectowner realneck frank', 'expectowner notebook frank', 'expectowner painting frank', 'expectowner rubaiyat frank')
    & tools/trace-boot.ps1 -Editions full -RestoreCheckpoint $RestoreCheckpoint -SavePath "$run/saves" -Steps 1 -Actions (@('expectview View250', 'expectglobal mission 4', 'expectglobal phase 1') + $inventory + @('key rightarrow', 'expectview View256', 'dumppreview', 'revisitscene latest', 'expectview View250', 'expectglobal mission 4', 'expectglobal phase 1') + $inventory + @('dumppreview')) *> "$run/suite.log"
    Copy-Item -LiteralPath build/boot-full.log -Destination "$run/native.log"
    $log = Get-Content "$run/native.log" -Raw
    if ([regex]::Matches($log, 'BOOT PREVIEW location=.* / View250 clock=\d+:\d+ picture=36864 items=\d+').Count -ne 2) { throw 'Saved checkpoint picture, location and clock evidence missing' }
    foreach ($item in @('realneck', 'notebook', 'painting', 'rubaiyat')) {
        if ([regex]::Matches($log, ('BOOT PREVIEW ITEM name=' + $item + ' owner=frank')).Count -ne 2) { throw "Saved item evidence missing: $item" }
    }
    @{scope='Earned Titanic sinking state; preview captured before normal turn and read before/after route rewind. Saved thumbnail dimensions, location, clock and all four ending-critical item owners checked without story/inventory injection.'; passed=$true; binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content "$run/proof.json"
    Write-Output "Titanic saved checkpoint preview and inventory rewind PASS: $run"
} finally { Pop-Location }
