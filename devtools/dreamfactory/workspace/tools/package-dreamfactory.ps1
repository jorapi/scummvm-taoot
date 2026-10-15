# SPDX-License-Identifier: GPL-3.0-or-later
# Package native Windows binaries and a source overlay; never include game data.
param([string]$OutputDirectory="$PSScriptRoot/../build/releases/platforms")
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=[IO.Path]::GetFullPath($OutputDirectory)
$windows=Join-Path $out 'windows-x64'
New-Item -ItemType Directory -Force $windows | Out-Null
foreach($file in @('scummvm.exe','SDL2.dll')) {
    Copy-Item -LiteralPath "$root/build/scummvm/Release/$file" -Destination $windows
}
foreach($file in @('COPYING','COPYRIGHT')) { Copy-Item -LiteralPath "$root/scummvm/$file" -Destination $windows }
Copy-Item -LiteralPath "$root/scummvm/dists/engine-data/fonts.dat" -Destination $windows
Copy-Item -Path "$root/scummvm/gui/themes/scumm*.zip" -Destination $windows
Copy-Item -LiteralPath "$root/scummvm/devtools/dreamfactory/README.md" -Destination "$windows/PLATFORMS.md"
Copy-Item -LiteralPath "$root/scummvm/devtools/dreamfactory/ENHANCEMENTS.md" -Destination $windows
Copy-Item -LiteralPath "$root/scummvm/devtools/dreamfactory/Run-Software-Renderer.cmd" -Destination $windows
Copy-Item -LiteralPath "$root/dependencies/SDL2-2.32.10/LICENSE.txt" -Destination "$windows/SDL2-LICENSE.txt"
Copy-Item -LiteralPath "$root/dependencies/freetype-VER-2-13-3/docs/FTL.TXT" -Destination "$windows/FREETYPE-LICENSE.txt"
Copy-Item -LiteralPath "$root/build/runtime-data/Liberation-LICENSE.txt" -Destination "$windows/LIBERATION-LICENSE.txt"
Compress-Archive -Path "$windows/*" -DestinationPath "$out/dreamfactory-windows-x64-unsigned.zip" -Force
$androidArtifacts=Join-Path $root 'scummvm/build-dreamfactory/artifacts'
if(Test-Path -LiteralPath $androidArtifacts) {
    Get-ChildItem -LiteralPath $androidArtifacts -Filter 'dreamfactory-android-*.apk' -File | Copy-Item -Destination $out
}
# Zip only the explicitly named source trees, preserving ScummVM-relative paths.
$env:DREAMFACTORY_PACKAGE_ROOT=$root
$env:DREAMFACTORY_PACKAGE_OUT=$out
try {
@'
import hashlib,json,os,pathlib,zipfile
root=pathlib.Path(os.environ['DREAMFACTORY_PACKAGE_ROOT'])
out=pathlib.Path(os.environ['DREAMFACTORY_PACKAGE_OUT'])
source=root/'scummvm'
files=list((source/'engines/dreamfactory').rglob('*'))+list((source/'devtools/dreamfactory').rglob('*'))+[source/'.github/workflows/dreamfactory-builds.yml']
with zipfile.ZipFile(out/'dreamfactory-source-overlay.zip','w',zipfile.ZIP_DEFLATED) as archive:
 for p in files:
  if p.is_file():archive.write(p,p.relative_to(source).as_posix())
artifacts=[]
names=['dreamfactory-windows-x64-unsigned.zip','dreamfactory-source-overlay.zip']+[p.name for p in sorted(out.glob('dreamfactory-android-*.apk'))]
for name in names:
 p=out/name;artifacts.append({'file':name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size})
manifest={'scope':'Available native packages and ScummVM-relative source overlay; see PLATFORMS.md for platform verification status. No game data included.','baseScummVMRevision':'c6777a499f23247b91469ea86b361dd8a655f4e0','windowsBinarySha256':hashlib.sha256((root/'build/scummvm/Release/scummvm.exe').read_bytes()).hexdigest(),'artifacts':artifacts}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Packages:',out)
'@ | python -
    if($LASTEXITCODE) { throw 'Source packaging failed' }
} finally {
    Remove-Item Env:DREAMFACTORY_PACKAGE_ROOT,Env:DREAMFACTORY_PACKAGE_OUT -ErrorAction SilentlyContinue
}
