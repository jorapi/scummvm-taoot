# Carry a developed or spoiled photograph to Penny and reach the authored ending.
param([int]$TimeoutSeconds = 600, [switch]$Spoiled)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/verify-demo-photograph.ps1" -TimeoutSeconds $TimeoutSeconds -Spoiled:$Spoiled -Choices @(
    'dpenny.pup:101', 'dpenny.pup:101'
) -ExtraActions @(
    'clickon button photoexit', 'expectflat main 1', 'expectowner photo1 frank',
    'key rightarrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'expectview View18',
    'clickon painting halla', 'key uparrow', 'expectset halla', 'expectview View44',
    'key leftarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'key uparrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'expectview View25', 'clickon painting gstair2,decka',
    'key uparrow', 'expectset gstair2', 'expectview View27',
    'key uparrow', 'key rightarrow', 'key rightarrow', 'key uparrow',
    'key leftarrow', 'key leftarrow', 'key uparrow', 'key leftarrow',
    'key uparrow', 'key rightarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'expectset gstair1', 'expectview View27',
    'key leftarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'expectscene Scene18', 'expectview View38',
    'clickon actor penny', 'expectglobal phase 2', 'expectflat demo 1', 'expectset none', 'savecheckpoint'
)
$root = Split-Path $PSScriptRoot -Parent
$trace = Get-Content "$root/build/gameplay-demo.log" -Raw
if ($trace -notmatch 'MOVIE PLAY postdemo\.mov') { throw 'Demo ending movie was not played' }
$variant = if ($Spoiled) { 'spoiled' } else { 'developed' }
Copy-Item "$root/build/gameplay-demo.log" "$root/build/demo-completion-$variant-native.log"
Copy-Item "$root/build/checkpoint-demo.dfsv" "$root/build/save-demo-completion-$variant.dfsv"
& "$PSScriptRoot/trace-boot.ps1" -Editions demo -RestoreCheckpoint "$root/build/save-demo-completion-$variant.dfsv" -TimeoutSeconds $TimeoutSeconds -Actions @(
    'expectglobal phase 2','expectset none','expectflat demo 1'
)
Copy-Item "$root/build/boot-demo.log" "$root/build/demo-completion-$variant-restore-native.log"
Write-Output "Demo story completion with $variant photograph PASS"
