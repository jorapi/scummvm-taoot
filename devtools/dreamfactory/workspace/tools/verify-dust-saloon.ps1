# Talk to Leroy at the shooting range, then enter the Hard Drive saloon.
param([string[]]$ExtraActions = @(), [string[]]$Choices = @(), [int]$TimeoutSeconds = 420)
$ErrorActionPreference = 'Stop'
$saloonActions = @(
    'key rightarrow', 'expectview east',
    'key uparrow', 'key uparrow', 'key uparrow', 'key uparrow',
    'expectscene Scene K11', 'key rightarrow', 'expectview south',
    'waitactorstar leroy town.leroy2', 'clickon actor leroy', 'expectglobal leroyphase 88',
    'key rightarrow', 'expectview west',
    'key uparrow', 'key uparrow', 'key uparrow', 'key uparrow',
    'expectscene Scene G11', 'key rightarrow', 'expectview north',
    'key uparrow', 'key uparrow', 'key uparrow', 'expectscene Scene G8',
    'key leftarrow', 'expectview west', 'click 270 100', 'expectowner door saloon',
    'key uparrow', 'expectset sallower', 'expectglobal saloonphase 1',
    'expectglobal townscene Scene G8', 'expectglobal theset sallower'
) + $ExtraActions
& "$PSScriptRoot/verify-dust-cards.ps1" -Choices $Choices -ExtraActions $saloonActions -TimeoutSeconds $TimeoutSeconds
