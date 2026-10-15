# SPDX-License-Identifier: GPL-3.0-or-later
# Exercise failed bath timing and recover by loading an earned pre-bath save.
param([string]$RestoreCheckpoint = "$PSScriptRoot/../build/save-demo-photograph.dfsv", [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
function Trace-Photo([string]$Name, [string]$Source, [string[]]$Actions) {
    & "$PSScriptRoot/trace-boot.ps1" -Editions demo -RestoreCheckpoint $Source -Checkpoint "$root/build/save-demo-photo-$Name.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions ($Actions + @('savecheckpoint'))
    Copy-Item "$root/build/boot-demo.log" "$root/build/demo-photo-$Name-native.log"
}
Trace-Photo 'retry-ready' $RestoreCheckpoint @(
    'expectowner photo1 frank','expectpropvalue photo1 1',
    'clickon painting photo','expectflat redphoto.stg',
    'clickon button redlight','waitpropview redlamp redon'
)
$ready = "$root/build/save-demo-photo-retry-ready.dfsv"
Trace-Photo 'wrong-bath' $ready @(
    'clickon button case2','waitpropview case2 openpic',
    'dragto prop case2 button stop','expectpropview pic2 under2a',
    'expectowner pic2 bad','waitpropview pic2 over2b',
    'clickon prop pic2','expectpropview pic2 badbig',
    'clickon prop pic2','expectpropview pic2 badhang',
    'dragto prop pic2 prop photobag','expectowner photo2 frank',
    'expectpropvalue photo2 0','expectpropdeg photo2 1',
    'expectowner photo1 frank','expectpropvalue photo1 1'
)
Trace-Photo 'wrong-bath-restored' "$root/build/save-demo-photo-wrong-bath.dfsv" @(
    'expectflat redphoto.stg','expectowner photo2 frank',
    'expectpropvalue photo2 0','expectpropdeg photo2 1',
    'clickon button photoexit','expectflat main 1',
    'key rightarrow','key leftarrow','expectview View33'
)
Trace-Photo 'overdeveloped' $ready @(
    'clickon button case3','waitpropview case3 openpic',
    'dragto prop case3 button start','expectpropview pic3 under1a',
    'waitpropview pic3 over2a',
    'clickon prop pic3','expectpropview pic3 badbig',
    'clickon prop pic3','expectpropview pic3 badhang',
    'dragto prop pic3 prop photobag','expectowner photo3 frank',
    'expectpropvalue photo3 0','expectpropdeg photo3 1'
)
Trace-Photo 'overdeveloped-restored' "$root/build/save-demo-photo-overdeveloped.dfsv" @(
    'expectflat redphoto.stg','expectowner photo3 frank',
    'expectpropvalue photo3 0','expectpropdeg photo3 1',
    'clickon button photoexit','expectflat main 1'
)
Trace-Photo 'retry-in-bath' $ready @(
    'clickon button case2','waitpropview case2 openpic',
    'dragto prop case2 button start','expectpropview pic2 under1a'
)
Trace-Photo 'retry-success' "$root/build/save-demo-photo-retry-in-bath.dfsv" @(
    'expectflat redphoto.stg','expectpropview pic2 under1a',
    'waitpropview pic2 good1','dragto prop pic2 button stop',
    'expectpropview pic2 under2b','waitpropview pic2 good2',
    'clickon prop pic2','expectpropview pic2 goodbig',
    'clickon prop pic2','expectpropview pic2 goodhang',
    'dragto prop pic2 prop photobag','expectowner photo2 frank',
    'expectpropvalue photo2 1','expectpropdeg photo2 0',
    'expectowner photo1 frank','expectpropvalue photo1 1',
    'clickon button photoexit','expectflat main 1'
)
Trace-Photo 'retry-success-restored' "$root/build/save-demo-photo-retry-success.dfsv" @(
    'expectowner photo1 frank','expectpropvalue photo1 1',
    'expectowner photo2 frank','expectpropvalue photo2 1','expectpropdeg photo2 0',
    'key rightarrow','key leftarrow','expectview View33'
)
Write-Output 'Demo wrong bath, overdevelopment, spoiled inventory persistence and in-bath saved retry PASS'
