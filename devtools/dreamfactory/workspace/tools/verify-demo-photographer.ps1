# Follow Penny's assignment to Burns and learn why his darkroom is unavailable.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$photographerChoices = @('dburns.pup:101', 'dburns.pup:103', 'dburns.pup:104') + $Choices
$photographerActions = @(
    'key leftarrow', 'key leftarrow', 'key uparrow', 'key leftarrow', 'key leftarrow',
    'expectview View26', 'key uparrow', 'expectset gstair2',
    'expectscene Scene50', 'expectview View54',
    'key leftarrow', 'key uparrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'expectscene Scene12', 'expectview View26',
    'clickon painting halla', 'expectowner door gs2-hallastar',
    'key uparrow', 'expectset halla', 'expectscene Scene20', 'expectview View26',
    'key uparrow', 'key rightarrow', 'key rightarrow', 'key uparrow',
    'key leftarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'key uparrow', 'key leftarrow', 'expectscene Scene24', 'expectview View43',
    'clickon painting b59', 'expectglobal burnsphase tryagain', 'expectglobal burnslike 1'
) + $ExtraActions
& "$PSScriptRoot/verify-demo-mission.ps1" -Choices $photographerChoices -ExtraActions $photographerActions -TimeoutSeconds $TimeoutSeconds
