# SPDX-License-Identifier: GPL-3.0-or-later
# Use the default thumbnail grid and its separate name dialog via real input.
param([int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$run = [Guid]::NewGuid().ToString('N')
foreach ($edition in 'full','demo','dust') {
    $checkpoint = if ($edition -eq 'dust') { 'day5-morning' } else { 'opening' }
    $view = switch ($edition) { full { 'View14' } demo { 'View55' } dust { 'south' } }
    $turned = switch ($edition) { full { 'View16' } demo { 'View71' } dust { 'west' } }
    $savedir = "$root/build/grid-saves-$run-$edition"
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-$checkpoint.dfsv" -SavePath $savedir -Chooser grid -TimeoutSeconds $TimeoutSeconds -GuiKeys @(
        's','gridfirst','click:SavenameDialog.Cancel','escape','escape',
        's','gridfirst','c','enter','l','gridfirst'
    ) -Actions @('menugui',"expectview $view",'menugui','expectsave 1 c',
        'key rightarrow',"expectview $turned",'menugui',"expectview $view",'key rightarrow',"expectview $turned")
    Copy-Item "$root/build/boot-$edition.log" "$root/build/grid-chooser-$edition-native.log"
    & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint "$root/build/save-$edition-$checkpoint.dfsv" -SavePath $savedir -TimeoutSeconds $TimeoutSeconds -Actions @(
        'expectsave 1 c','loadslot 1',"expectview $view",'key rightarrow',"expectview $turned"
    )
    Copy-Item "$root/build/boot-$edition.log" "$root/build/grid-chooser-$edition-fresh-native.log"
    Write-Output "$edition grid chooser name cancellation, named save, thumbnail load and fresh restore PASS; saves=$savedir"
}
