# SPDX-License-Identifier: GPL-3.0-or-later
# Native input and file round trips; never injects game globals or inventory.
param([switch]$Story, [int]$TimeoutSeconds = 600)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$previousSpeech = $env:DREAMFACTORY_BOOT_SKIP_SPEECH
try {
    $env:DREAMFACTORY_BOOT_SKIP_SPEECH = '1'
    foreach ($edition in 'full','demo','dust') {
        $expectedView = switch ($edition) { full { 'View14' }; demo { 'View55' }; dust { 'north' } }
        $roundtrip = @('savecheckpoint', 'key rightarrow', 'loadcheckpoint', "expectview $expectedView")
        if ($edition -eq 'dust') {
            & "$PSScriptRoot/verify-dust-opening.ps1" -ExtraActions $roundtrip -TimeoutSeconds $TimeoutSeconds *> "$root/build/save-$edition-opening.log"
        } else {
            & "$PSScriptRoot/verify-gameplay.ps1" -Editions $edition -ExtraActions $roundtrip -TimeoutSeconds $TimeoutSeconds *> "$root/build/save-$edition-opening.log"
        }
        $checkpoint = "$root/build/save-$edition-opening.dfsv"
        Copy-Item -LiteralPath "$root/build/checkpoint-$edition.dfsv" -Destination $checkpoint
        $slot = switch ($edition) { full { 90 }; demo { 91 }; dust { 92 } }
        & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -RestoreCheckpoint $checkpoint -Actions @(
            "expectview $expectedView", "saveslot $slot", 'key rightarrow', "loadslot $slot", "expectview $expectedView"
        ) *> "$root/build/save-$edition-fresh-slot.log"
        # Click region centers: Titanic switch artwork also extends outside its
        # button, where a physical click means dragging the switch handle.
        if ($edition -eq 'dust') {
            $controls = @('clickon button horn','expectflat score','click 252 196','expectflat score',
                'click 252 322','expectflat mainpanel','key rightarrow','clickon button horn',
                'expectflat score','click 252 228','expectflat mainpanel','expectview north',
                'expectglobal leroyphase 1','key rightarrow','expectview east')
        } else {
            $controls = @('clickon prop life')
            if ($edition -eq 'full') { $controls += 'clickon prop life' }
            $controls += @('expectflat ctl 1','click 130 48','expectflat ctl 1','click 130 328',
                'expectflat main 1','key rightarrow','clickon prop life','expectflat ctl 1',
                'click 130 104','expectflat main 1',"expectview $expectedView",'key rightarrow')
        }
        & "$PSScriptRoot/trace-boot.ps1" -Editions $edition -DialogSlot $slot -RestoreCheckpoint $checkpoint -Actions $controls *> "$root/build/save-$edition-control.log"
        Copy-Item -LiteralPath "$root/build/boot-$edition.log" -Destination "$root/build/save-$edition-control-native.log"
        Write-Output "$edition opening, fresh process, ScummVM slot and authored control-panel round trips PASS"
    }
    & python "$PSScriptRoot/save-fixtures.py" "$root/build/save-full-opening.dfsv" "$root/build/save-invalid"
    if ($LASTEXITCODE) { throw 'Save fixture generation failed' }
    foreach ($fixture in Get-ChildItem -LiteralPath "$root/build/save-invalid" -Filter '*.dfsv') {
        & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-opening.dfsv" -Checkpoint $fixture.FullName -Actions @(
            'rejectcheckpoint','expectset bedsit1','expectview View14','key rightarrow','expectview View16'
        ) *> "$root/build/save-reject-$($fixture.BaseName).log"
    }
    & "$PSScriptRoot/trace-boot.ps1" -Editions demo -RestoreCheckpoint "$root/build/save-demo-opening.dfsv" -Checkpoint "$root/build/save-full-opening.dfsv" -Actions @(
        'rejectcheckpoint','expectset c71','expectview View55'
    ) *> "$root/build/save-reject-cross-edition.log"
    Write-Output 'Invalid and cross-edition save rejection PASS'
    if ($Story) {
        & "$PSScriptRoot/verify-full-mission1.ps1" -BeforeDeliveryActions @('savecheckpoint') -ExtraActions @(
            'loadcheckpoint','expectset halla','expectscene Scene51','expectview View58',
            'expectowner vladfood frank','expectowner rubaiyat coal5','expectglobal mission 1',
            'expectglobal phase 3','expectglobal playerdeath '
        ) -TimeoutSeconds $TimeoutSeconds *> "$root/build/save-full-story-roundtrip.log"
        Copy-Item -LiteralPath "$root/build/checkpoint-full.dfsv" -Destination "$root/build/save-full-sasha.dfsv"
        Copy-Item -LiteralPath "$root/build/boot-full.log" -Destination "$root/build/save-full-story-native.log"
        $prefix = 'dreamfactory_boot_actions='
        $line = (Get-Content "$root/build/boot-full.ini" | Where-Object { $_.StartsWith($prefix) }).Substring($prefix.Length)
        $actions = @($line -split ';')
        $first = [array]::IndexOf($actions, 'savecheckpoint')
        $last = [array]::IndexOf($actions, 'loadcheckpoint')
        if ($first -lt 0 -or $last -le $first) { throw 'Story checkpoint markers missing' }
        & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-sasha.dfsv" -Checkpoint "$root/build/save-full-mission2.dfsv" -SkipSpeech -ClickMovies @('sasha.mov') -Choices @(
            'vlad1.pup:101','penny1.pup:101','penny1.pup:104','penny1.pup:102','penny1.pup:105','penny1.pup:109'
        ) -Actions (@($actions[($first+1)..($last-1)]) + @('savecheckpoint')) -TimeoutSeconds $TimeoutSeconds *> "$root/build/save-full-fresh-story.log"
        Copy-Item -LiteralPath "$root/build/boot-full.log" -Destination "$root/build/save-full-fresh-story-native.log"
        & "$PSScriptRoot/trace-boot.ps1" -Editions full -RestoreCheckpoint "$root/build/save-full-mission2.dfsv" -Actions @(
            'expectset hallf2c','expectview View76','expectglobal mission 2','expectglobal phase 0',
            'expectowner rubaiyat frank','expectowner vladfood vlad','expectglobal playerdeath ','key rightarrow'
        ) *> "$root/build/save-full-mission2-restore.log"
        Copy-Item -LiteralPath "$root/build/boot-full.log" -Destination "$root/build/save-full-mission2-restore-native.log"
        Write-Output 'Retail checkpoint rollback and fresh-process mission-one completion PASS'
    }
} finally {
    $env:DREAMFACTORY_BOOT_SKIP_SPEECH = $previousSpeech
}
