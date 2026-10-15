# Find Burns's wife, report her location, and enter his darkroom safely.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 600)
$ErrorActionPreference = 'Stop'
$darkroomChoices = @('dburns.pup:106', 'dburns.pup:102') + $Choices
$darkroomActions = @(
    'key rightarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'key uparrow', 'key rightarrow', 'key uparrow',
    'key rightarrow', 'key rightarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'expectscene Scene20', 'expectview View25',
    'clickon painting gstair2,decka', 'expectowner door halla-gs2star',
    'key uparrow', 'expectset gstair2', 'expectview View27',
    'key rightarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'key rightarrow', 'key rightarrow', 'key uparrow', 'key rightarrow',
    'expectscene Scene11', 'expectview View23',
    'key leftarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'key rightarrow', 'key rightarrow', 'key uparrow', 'key rightarrow',
    'expectview View26', 'clickon painting halla', 'expectowner door gs2-hallastar',
    'key uparrow', 'expectset halla', 'expectview View26',
    'key uparrow', 'key rightarrow', 'key rightarrow', 'key uparrow',
    'key leftarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'key uparrow', 'key leftarrow', 'expectscene Scene24', 'expectview View43',
    'clickon painting b59', 'expectglobal burnsphase left',
    'clickon painting b59', 'expectowner door halla-b59',
    'key uparrow', 'expectset b59', 'expectview View19',
    'key rightarrow', 'expectview View20', 'clickon painting light', 'expectglobal whitelight off',
    'key leftarrow', 'key uparrow', 'key rightarrow', 'key rightarrow',
    'key uparrow', 'key rightarrow', 'key rightarrow', 'expectview View33',
    'clickon painting photo', 'expectflat redphoto.stg',
    'clickon button redlight', 'waitpropview redlamp redon',
    'clickon button case1', 'waitpropview case1 openpic'
) + $ExtraActions
& "$PSScriptRoot/verify-demo-photographer.ps1" -Choices $darkroomChoices -ExtraActions $darkroomActions -TimeoutSeconds $TimeoutSeconds
