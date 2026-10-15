# Authored opening interactions. Uses normal input, never script state injection.
param([ValidateSet('demo','full','dust')][string[]]$Editions = @('demo','full','dust'), [string[]]$ExtraActions = @(), [string[]]$Choices = @(), [string[]]$ClickMovies = @(), [string[]]$SkipMovies = @(), [int]$TimeoutSeconds = 300)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
foreach ($edition in $Editions) {
    $options = @{ Editions = @($edition); Steps = 40; TimeoutSeconds = $TimeoutSeconds; Choices = $Choices }
    switch ($edition) {
        demo {
            $options.ClickX = 260
            $options.ClickY = 190
            $options.ClickMovies = @('date.mov')
            $options.SkipMovies = @('open.mov','flyby.mov','itrunkey.mov')
            $options.Actions = @(
                'expectset c71','clickon prop bag','expectowner bag frank','expectowner trunkkey frank',
                'expectpropview bag darkclosed','clickon prop bag','expectpropview bag lightclosed',
                'clickon prop bag','idle 10','expectflat inven 1','clickon prop trunkkey',
                'expectglobal handitem trunkkey','clickon button examine','expectflat inven 1',
                'expectglobal handitem trunkkey','clickon button ok','expectflat main 1',
                'expectpropview trunkkey large','key leftarrow','key leftarrow','key leftarrow',
                'key leftarrow','key leftarrow','expectview View66',
                'dragto prop trunkkey painting trunk','expectflat trunk 1',
                'clickon prop grammy','dragto prop horn button hornup','expectpropview horn up',
                'clickon button gramdrawerbut','idle 10','expectpropview gramdrawer idle1',
                'expectpropvalue gramdrawer 1','clickon button ok','expectflat trunk 1',
                'clickon button ok','expectflat main 1','key rightarrow','key rightarrow',
                'key rightarrow','expectview View61','clickon prop watch','expectowner watch frank',
                'clickon prop map','expectowner map frank','key rightarrow','key rightarrow',
                'expectview View63','key uparrow','expectscene Scene49','expectview View55'
            )
        }
        full {
            $options.ChooseGame = $true
            $options.SkipMovies = @('logo.mov','datebed.mov')
            $options.Actions = @('expectset bedsit1','expectflat main 1','key rightarrow','key leftarrow','expectview View14')
        }
        dust {
            $options.ChooseGame = $true
            $options.SkipMovies = @('intro.mov','intro2.mov','intro3.mov')
            $options.Actions = @(
                'expectset town','clickon button map','expectflat map','clickon button Untitled',
                'expectflat mainpanel','clickon prop avatar','expectflat avatar','idle 10',
                'clickon button OK','expectflat mainpanel','key rightarrow','expectview east',
                'clickon prop avatar','expectflat avatar','idle 10'
            )
        }
    }
    $options.Actions += $ExtraActions
    if ($ClickMovies.Count) { $options.ClickMovies = @($options.ClickMovies) + $ClickMovies }
    if ($SkipMovies.Count) { $options.SkipMovies = @($options.SkipMovies) + $SkipMovies }
    & "$PSScriptRoot/trace-boot.ps1" @options
    Copy-Item "$root/build/boot-$edition.log" "$root/build/gameplay-$edition.log"
    Copy-Item "$root/build/boot-$edition.rgba" "$root/build/gameplay-$edition.rgba"
}
