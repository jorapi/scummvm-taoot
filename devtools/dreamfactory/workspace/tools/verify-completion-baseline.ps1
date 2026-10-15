# SPDX-License-Identifier: GPL-3.0-or-later
# Version-pinned regression evidence, not a declaration of full playability.
param(
    [ValidateSet('Components','Story','Controls','All')][string]$Suite = 'Components',
    [ValidateSet('images','script','Resources','Cursors','Rooms','Stars','Stages','Shops','Screens','Movies','Puppets','Casts','Audio')][string[]]$Component = @()
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $run = Join-Path $root ('build/completion-baseline/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $run -Force | Out-Null
    function Get-BaselineHashes {
        $files = @(
            Get-ChildItem scummvm/engines/dreamfactory -File -Recurse
            Get-ChildItem tools -File
            Get-Item build/scummvm/Release/scummvm.exe
        )
        @($files | Sort-Object FullName | ForEach-Object {
            [ordered]@{ path = [IO.Path]::GetRelativePath($root, $_.FullName); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        })
    }
    $initial = Get-BaselineHashes
    $manifest = [ordered]@{
        scope = 'Regression baseline. Passing these suites alone does not establish every branch or full playability. Resumed earned checkpoints do not independently establish their startup-to-checkpoint history.'
        demo = 'Supplied September 1996 interactive demo'
        started = (Get-Date).ToUniversalTime().ToString('o')
        suite = $Suite
        selectedComponents = $Component
        initialHashes = $initial
        checkpointInputs = @(Get-ChildItem build -File -Filter '*.dfsv' | ForEach-Object {
            @{ path=[IO.Path]::GetRelativePath($root,$_.FullName); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        })
        results = @()
        unchanged = $null
    }
    $jobs = @()
    if ($Suite -in 'Components','All') {
        if (!$Component.Count -or $Component -contains 'images') { $jobs += @{ name='images'; file='verify.ps1'; arguments=@{} } }
        foreach ($kind in 'script','Resources','Cursors','Rooms','Stars','Stages','Shops','Screens','Movies','Puppets','Casts','Audio') {
            if ($Component.Count -and $Component -notcontains $kind) { continue }
            $arguments = @{}
            if ($kind -ne 'script') { $arguments[$kind] = $true }
            $jobs += @{ name=$kind; file='verify-scripts.ps1'; arguments=$arguments; marker=$kind.ToLowerInvariant().TrimEnd('s') }
        }
    }
    if ($Suite -in 'Story','All') {
        $jobs += @{ name='retail-best-ending'; file='verify-full-best-ending.ps1'; arguments=@{} }
        $jobs += @{ name='retail-ending-return'; file='verify-full-ending-return.ps1'; arguments=@{} }
        $jobs += @{ name='demo-developed'; file='verify-demo-completion.ps1'; arguments=@{} }
        $jobs += @{ name='demo-spoiled'; file='verify-demo-completion.ps1'; arguments=@{ Spoiled=$true } }
        $jobs += @{ name='demo-media'; file='verify-demo-menu-media.ps1'; arguments=@{} }
        foreach ($ending in 'yunni','ranch','mayor','marie','desert') {
            $jobs += @{ name="dust-ending-$ending"; file='verify-dust-ending.ps1'; arguments=@{ Ending=$ending; FullCredits=($ending -eq 'yunni') } }
        }
    }
    if ($Suite -in 'Controls','All') {
        foreach ($item in @(
            @('launch','verify-launch.ps1'), @('save','verify-save-state.ps1'),
            @('settings','verify-settings-controls.ps1'), @('menu','verify-game-menu.ps1'),
            @('dialogs','verify-script-dialogs.ps1'), @('death-recovery','verify-death-recovery.ps1')
        )) { $jobs += @{ name=$item[0]; file=$item[1]; arguments=@{} } }
    }
    # Deliberately serial: traces share per-edition files and some suites regenerate
    # the same movie fixtures. Each suite's evidence is copied before the next runs.
    foreach ($job in $jobs) {
        $started = Get-Date
        $dest = Join-Path $run $job.name
        New-Item -ItemType Directory -Path $dest | Out-Null
        $passed = $false
        try {
            $arguments = $job.arguments
            & (Join-Path $PSScriptRoot $job.file) @arguments *> "$dest/suite.log"
            if ($job.ContainsKey('marker')) {
                $output = Get-Content "$dest/suite.log" -Raw
                foreach ($edition in 'demo','full','dust') {
                    if ($output -notmatch "(?m)^$edition $($job.marker) PASS\s*$") {
                        throw "Missing $edition $($job.marker) suite result; check flag forwarding"
                    }
                }
            }
            $passed = $true
        } catch { $_ | Out-String | Add-Content "$dest/suite.log" }
        $logs = @(Get-ChildItem build -File -Filter '*.log' | Where-Object { $_.LastWriteTime -ge $started })
        foreach ($log in $logs) { Copy-Item -LiteralPath $log.FullName -Destination $dest }
        $manifest.results += [ordered]@{
            name=$job.name; passed=$passed; started=$started.ToUniversalTime().ToString('o')
            finished=(Get-Date).ToUniversalTime().ToString('o')
            logs=@(Get-ChildItem $dest -File | ForEach-Object {
                @{ path=[IO.Path]::GetRelativePath($run,$_.FullName); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
            })
        }
        $manifest | ConvertTo-Json -Depth 10 | Set-Content "$run/manifest.json"
        Write-Output "$($job.name) passed=$passed evidence=$dest"
    }
    $final = Get-BaselineHashes
    $manifest.unchanged = (($initial | ConvertTo-Json -Depth 4 -Compress) -ceq ($final | ConvertTo-Json -Depth 4 -Compress))
    $manifest['finalHashes'] = $final
    $manifest['finished'] = (Get-Date).ToUniversalTime().ToString('o')
    $manifest | ConvertTo-Json -Depth 10 | Set-Content "$run/manifest.json"
    if (!$manifest.unchanged) { throw "Sources, tools or executable changed during validation: $run" }
    if (@($manifest.results | Where-Object { !$_.passed }).Count) { throw "Baseline failures: $run" }
    Write-Output "Baseline PASS: $run/manifest.json"
} finally { Pop-Location }
