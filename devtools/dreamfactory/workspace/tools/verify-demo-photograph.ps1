# Collect the Conkling photograph, correctly developed or deliberately spoiled.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 600, [switch]$Spoiled)
$ErrorActionPreference = 'Stop'
$photographActions = @(
    'dragto prop case1 button start', 'expectpropview pic1 under1a',
    'waitpropview pic1 good1', 'dragto prop pic1 button stop',
    'expectpropview pic1 under2b', 'waitpropview pic1 good2',
    'clickon prop pic1', 'expectpropview pic1 goodbig',
    'clickon prop pic1', 'expectpropview pic1 goodhang',
    'dragto prop pic1 prop photobag', 'expectowner photo1 frank',
    'expectpropvalue photo1 1'
)
if ($Spoiled) {
    # The original accepts a spoiled Conkling photo with different dialogue.
    $photographActions = @(
        'dragto prop case1 button stop','expectpropview pic1 under2a',
        'expectowner pic1 bad','waitpropview pic1 over2b',
        'clickon prop pic1','expectpropview pic1 badbig',
        'clickon prop pic1','expectpropview pic1 badhang',
        'dragto prop pic1 prop photobag','expectowner photo1 frank',
        'expectpropvalue photo1 0','expectpropdeg photo1 1'
    )
}
$photographActions += $ExtraActions
& "$PSScriptRoot/verify-demo-darkroom.ps1" -Choices $Choices -ExtraActions $photographActions -TimeoutSeconds $TimeoutSeconds
