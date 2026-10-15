# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force dependencies, build/runtime-data | Out-Null
    function Get-CheckedArchive($Url, $File, $Sha256) {
        if (!(Test-Path -LiteralPath $File)) { Invoke-WebRequest -Uri $Url -OutFile $File }
        if ((Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash -ne $Sha256) { throw "Dependency hash mismatch: $File" }
    }
    Get-CheckedArchive 'https://codeload.github.com/freetype/freetype/zip/refs/tags/VER-2-13-3' 'dependencies/freetype-2.13.3.zip' 'A7165CB405DFCE349AE64D6F59EFB2567565CA8EDBACBEFF2A8D0E4CFA779CF5'
    if (!(Test-Path dependencies/freetype-VER-2-13-3/CMakeLists.txt)) { Expand-Archive -LiteralPath dependencies/freetype-2.13.3.zip -DestinationPath dependencies }
    Get-CheckedArchive 'https://github.com/liberationfonts/liberation-fonts/files/7261482/liberation-fonts-ttf-2.1.5.tar.gz' 'dependencies/liberation-fonts-ttf-2.1.5.tar.gz' '7191C669BF38899F73A2094ED00F7B800553364F90E2637010A69C0E268F25D0'
    if (!(Test-Path dependencies/liberation-fonts-ttf-2.1.5/LiberationSans-Regular.ttf)) {
        tar -xzf dependencies/liberation-fonts-ttf-2.1.5.tar.gz -C dependencies
        if ($LASTEXITCODE) { throw 'Font extraction failed' }
    }
    Copy-Item -LiteralPath dependencies/liberation-fonts-ttf-2.1.5/LiberationSans-Regular.ttf -Destination build/runtime-data/LiberationSans-Regular.ttf -Force
    Copy-Item -LiteralPath dependencies/liberation-fonts-ttf-2.1.5/LICENSE -Destination build/runtime-data/Liberation-LICENSE.txt -Force
    Copy-Item -LiteralPath dependencies/freetype-VER-2-13-3/docs/FTL.TXT -Destination build/runtime-data/FreeType-LICENSE.txt -Force
    'Portions of this software are copyright (C) 1996-2024 The FreeType Project (www.freetype.org). All rights reserved.' | Set-Content build/runtime-data/FreeType-NOTICE.txt
    cmake -S dependencies/freetype-VER-2-13-3 -B build/freetype -G 'Visual Studio 17 2022' -A x64 -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
    if ($LASTEXITCODE) { throw 'FreeType configuration failed' }
    cmake --build build/freetype --config Release --parallel 8
    if ($LASTEXITCODE) { throw 'FreeType build failed' }
} finally { Pop-Location }
