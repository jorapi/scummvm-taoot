# SPDX-License-Identifier: GPL-3.0-or-later
# Bounty hunters and the first Kid wave use randomized spawns and native hit targets.
param([string]$RestoreCheckpoint="$PSScriptRoot/../build/save-dust-bounty-active.dfsv", [int]$TimeoutSeconds=300, [switch]$Armed, [switch]$GangWave1, [string]$ApproachCell='', [ValidateSet('north','south','east','west')][string]$ApproachFacing='north')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$script:current=$RestoreCheckpoint
$script:sequence=0
$script:native=''
$actors=if($GangWave1) { @('kidgang1','kidgang3','kidgang2') } else { @('bounty4','bounty5','bounty2','bounty1','bounty3') }
$room=if($GangWave1) { 'TOWN' } else { 'NITE' }
$prefix=if($GangWave1) { 'kid-wave1-live' } else { 'bounty-live' }
$storyPhase=if($GangWave1) { 2 } else { 3 }
$scenes = & node --input-type=module -e "import fs from 'node:fs'; import {readSetFileV1} from './build/reference-room/set-v1.mjs'; console.log(JSON.stringify(readSetFileV1(new Uint8Array(fs.readFileSync('game-data/dust/$room.SET'))).scenes.map(s=>({name:s.name,x:s.x,y:s.z}))));" | ConvertFrom-Json
if($LASTEXITCODE) { throw 'Cannot read authored combat standpoints' }
function InRange([string]$Name) {
    if(!$GangWave1 -and $Name -notin @('bounty1','bounty2')) { return $true }
    $actor=[regex]::Match($script:native,('BOOT ACTOR name='+$Name+' .*? xyz=(-?\d+),(-?\d+),'))
    $location=[regex]::Match($script:native,'BOOT LOCATION room=town scene=(.*?) view=')
    $scene=$scenes | Where-Object name -eq $location.Groups[1].Value
    if(!$actor.Success -or !$scene) { throw 'Missing combat distance evidence' }
    $dx=[int]$actor.Groups[1].Value-([int]$scene.x*256+128)
    $dy=[int]$actor.Groups[2].Value-([int]$scene.y*256+128)
    # Walking hunters erase their damage in authored makemove(). Engage
    # inside hotdist(2), where their next loop enters attack mode instead.
    return ($dx*$dx+$dy*$dy) -lt (700*700)
}
function Battle([string]$Label,[string[]]$Actions,[string[]]$Choices=@()) {
    ++$script:sequence
    $next="$root/build/save-dust-$prefix-$($script:sequence)-$Label.dfsv"
    $inspect=@('expectglobal playerdeath ','savecheckpoint','targets')
    foreach($actor in $actors) { $inspect+="dumpactor $actor" }
    & "$root/tools/trace-boot.ps1" -Editions dust -RestoreCheckpoint $script:current -Checkpoint $next -Steps 1 -TimeoutSeconds $TimeoutSeconds -Choices $Choices -Actions ($Actions+$inspect)
    Copy-Item -LiteralPath "$root/build/boot-dust.log" -Destination "$root/build/dust-$prefix-$($script:sequence)-$Label-native.log"
    $script:current=$next
    $script:native=Get-Content "$root/build/boot-dust.log" -Raw
}
function Pose([string]$Name) {
    $match=[regex]::Match($script:native,('BOOT ACTOR name='+[regex]::Escape($Name)+' .*? pose=(\S+)'))
    if(!$match.Success) { throw "Missing native actor status: $Name" }
    $match.Groups[1].Value
}
function Aim([string]$Name) {
    $pattern='BOOT TARGET actor:'+ [regex]::Escape($Name) +' '
    # Narrow cardinal views and scenery can conceal a hunter at a diagonal.
    # Patrol real street standpoints after each full turn, rather than assuming
    # that waiting makes every random spawn visible from one fixed cell.
    $streets=@(@('G11','west'),@('G9','north'),@('I7','east'),@('B11','north'),@('G15','north'))
    for($scan=0;$scan -le $streets.Count;$scan++) {
        for($turn=0;$turn -lt 4;$turn++) {
            if($script:native -match $pattern) { return }
            Battle "aim-$Name-$scan-$turn" @('key leftarrow','idle 20')
        }
        if($script:native -match $pattern) { return }
        if($scan -lt $streets.Count) { MoveTo $streets[$scan][0] $streets[$scan][1] }
    }
    throw "Target did not become visible through ordinary turning: $Name"
}
function Ammo {
    $state=& python "$root/tools/read-checkpoint-globals.py" $script:current bulletcount | ConvertFrom-Json
    if($LASTEXITCODE -or $state.bulletcount -lt 0 -or $state.bulletcount -gt 6) { throw 'Cannot read the displayed ammunition count' }
    [int]$state.bulletcount
}
function Reload {
    $count=6-(Ammo)
    if(!$count) { return }
    $a=@('clickon prop gunhand','expectpropview gunhand reload')
    1..$count | ForEach-Object { $a+='click 234 235' }
    $a+=@('expectglobal bulletcount 6','clickon prop gunhand','expectpropview gunhand idle')
    Battle reload $a
}
function Shoot([string]$Name) {
    for($shot=0;$shot -lt 16;$shot++) {
        if((Pose $Name) -eq 'dead') { return }
        # A moving target can leave the view. Let the patrol choose another
        # visible opponent instead of waiting under fire for this one.
        if($script:native -notmatch ('BOOT TARGET actor:'+ [regex]::Escape($Name) +' ')) { return }
        if(!(InRange $Name)) { return }
        if(!(Ammo)) { Reload }
        Battle "shoot-$Name-$shot" @("clickon actor $Name",'waitpropview gunhand idle 10000')
    }
    if((Pose $Name) -ne 'dead') { throw "Target survived the physical shots: $Name" }
}
function MoveTo([string]$Cell,[string]$Facing) {
    $place=[regex]::Match($script:native,'BOOT LOCATION room=town scene=(.*?) view=(.*?) stage=')
    if(!$place.Success) { throw 'Missing native town standpoint' }
    $route=@(& node "$root/tools/dust-route.mjs" $room $place.Groups[1].Value $place.Groups[2].Value $Cell $Facing)
    if($LASTEXITCODE) { throw 'Cannot plan physical combat movement' }
    Battle "move-$Cell-$Facing" $route
}
if($Armed) { Battle armed @('expectglobal fighton 1',"expectglobal phase $storyPhase",'expectglobal handitem gun','expectpropview gunhand idle') }
else { Battle armed @('expectglobal fighton 1',"expectglobal phase $storyPhase",'clickon prop avatar','expectflat avatar','clickon prop gun','clickon button ok','expectglobal handitem gun','clickon prop gun','waitpropview gunhand idle 10000') }
if($ApproachCell) { MoveTo $ApproachCell $ApproachFacing }
elseif($GangWave1) { MoveTo G11 west } else { MoveTo G13 west }
$patrol=@(@('G11','west'),@('G9','north'),@('I7','east'),@('B11','north'),@('G15','north'),@('G13','west'))
if($GangWave1) { $patrol=@(@('C7','west'),@('D10','north'),@('G13','south'))+$patrol }
$turns=0
$waypoint=0
$defeated=$false
for($pass=0;$pass -lt 160;$pass++) {
    $alive=@($actors | Where-Object { (Pose $_) -ne 'dead' })
    if(!$alive.Count) { $defeated=$true; break }
    $visible=@($alive | Where-Object { ($script:native -match ('BOOT TARGET actor:'+ [regex]::Escape($_) +' ')) -and (InRange $_) })
    if($visible.Count) {
        Shoot $visible[0]
        if(@($actors | Where-Object { (Pose $_) -ne 'dead' }).Count) { Reload }
        $turns=0; continue
    }
    if($turns -lt 4) { Battle "scan-$pass" @('key leftarrow','idle 2'); ++$turns }
    else {
        $place=$patrol[$waypoint % $patrol.Count]
        MoveTo $place[0] $place[1]
        ++$waypoint
        $turns=0
    }
}
if(!$defeated) { throw 'Hunters were not defeated through visible physical shots' }
if($GangWave1) {
    Copy-Item -LiteralPath $script:current -Destination "$root/build/save-dust-kid-wave1-defeated.dfsv"
    Copy-Item -LiteralPath "$root/build/boot-dust.log" -Destination "$root/build/dust-kid-wave1-complete-native.log"
    Write-Output 'Dust randomized first Kid wave defeated through visible physical shots PASS'
    exit
}
Battle holster @('clickon prop gun','idle 10')
MoveTo G9 north
Battle trotter-ready @('idle 30')
Aim trotter
Battle reward @('clickon actor trotter','expectglobal fighton 0','expectglobal trotterphase 1') @('trotter.pup:101','trotter.pup:102','trotter.pup:103')
MoveTo G9 north
Copy-Item -LiteralPath $script:current -Destination "$root/build/save-dust-bounty-trotter.dfsv"
Write-Output 'Dust five-hunter fight, physical reloads, visible-target aiming and post-fight reward PASS'
