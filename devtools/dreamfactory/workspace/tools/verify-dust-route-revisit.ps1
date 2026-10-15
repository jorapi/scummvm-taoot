# SPDX-License-Identifier: GPL-3.0-or-later
# Replay Dust's earned finale checkpoint into two authored endings, without changing story state.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-dust-day5-morning.dfsv")
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/route-revisit-dust/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    $route = @(& node tools/dust-route.mjs TOWN G4 south G14 south)
    if ($LASTEXITCODE -or $route.Count -lt 3) { throw 'Cannot plan the authored finale approach' }
    $route = @($route[0..($route.Count - 3)])
    $actions = @('expectglobal day 5','expectglobal phase 0','expectowner chest stranger') + $route + @(
        'expectglobal phase 99', 'dumproutes', 'revisitscene latest',
        'expectglobal day 5', 'expectglobal phase 0', 'expectscene Scene G13', 'expectview south', 'expectowner chest stranger',
        'key uparrow', 'expectglobal phase 99', 'dumproutes', 'savecheckpoint'
    )
    & tools/trace-boot.ps1 -Editions dust -RestoreCheckpoint $RestoreCheckpoint -Checkpoint "$run/final.dfsv" -SavePath "$run/saves" -Steps 1 -TimeoutSeconds 300 -SkipSpeech -SkipMovies @('yunniend.mov','marieend.mov') -Choices @(
        'oona.pup:101','jones.pup:222','oona.pup:666',
        'oona.pup:101','jones.pup:222','oona.pup:555'
    ) -Actions $actions *> "$run/suite.log"
    if ($LASTEXITCODE) { throw "Native trace failed; inspect $run/suite.log" }
    Copy-Item -LiteralPath build/boot-dust.log -Destination "$run/native.log"
    $log = Get-Content "$run/native.log" -Raw
    $parts = $log -split 'BOOT ROUTE RESTORE PASS', 2
    if ($parts.Count -ne 2) { throw 'No successful route checkpoint restoration' }
    function Nodes($text) {
        $nodes = @{}
        foreach ($match in [regex]::Matches($text, 'BOOT ROUTE node=(\d+) kind=(\d+) parent=(\d+) choice=(-?\d+) seen=(\d+) checkpoint=(\d+) file=(\S*) speaker=([^\r\n]*)')) {
            $nodes[[int]$match.Groups[1].Value] = @{ kind=[int]$match.Groups[2].Value; parent=[int]$match.Groups[3].Value; seen=[int]$match.Groups[5].Value; file=$match.Groups[7].Value }
        }
        return $nodes
    }
    $first = Nodes $parts[0]; $second = Nodes $parts[1]
    $endings = @($second.Values | Where-Object kind -eq 2)
    if ($endings.Count -ne 2 -or !@($endings | Where-Object file -Match 'yunniend.mov$').Count -or !@($endings | Where-Object file -Match 'marieend.mov$').Count) { throw 'Both authored ending nodes were not retained' }
    $expanded = @($first.Keys | Where-Object { $first[$_].kind -eq 1 -and $second.ContainsKey($_) -and $second[$_].seen -ne $first[$_].seen -and ($second[$_].seen -band $first[$_].seen) -eq $first[$_].seen })
    if (!$expanded.Count -or $second.Count -lt $first.Count + 1 -or $second.Count -gt $first.Count + 2) { throw 'Replay duplicated its shared encounter or failed to discover the alternate reply' }
    @{ scope='Earned Dust day-five state; player navigation, Yunni ending, route checkpoint revisit and Marie ending; speech and ending movies skipped with normal input'; beforeNodes=$first.Count; afterNodes=$second.Count; expandedDecisions=$expanded; passed=$true; binarySha256=(Get-FileHash build/scummvm/Release/scummvm.exe -Algorithm SHA256).Hash } | ConvertTo-Json -Depth 4 | Set-Content "$run/proof.json"
    Write-Output "Dust route checkpoint, shared encounter replay and alternate ending PASS: $run"
} finally { Pop-Location }
