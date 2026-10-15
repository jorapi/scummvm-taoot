$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    & "$PSScriptRoot/prepare-fonts.ps1"
    cmake -S scummvm/devtools/create_project/cmake -B build/create-project -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE) { throw 'create_project configuration failed' }
    cmake --build build/create-project --config Release
    if ($LASTEXITCODE) { throw 'create_project build failed' }
    $generator = Join-Path $root 'build/create-project/Release/create_project.exe'
    $helpText = & $generator
    $disable = $helpText | Select-String '^\s+enabled\s+\|\s+(\S+)' | ForEach-Object { '--disable-' + $_.Matches[0].Groups[1].Value }
    New-Item -ItemType Directory -Force build/generated | Out-Null
    Push-Location build/generated
    try {
        & $generator ../../scummvm --cmake --disable-all-engines --enable-engine=dreamfactory @disable --enable-opengl --enable-builtin-resources --enable-freetype2 --enable-highres --enable-16bit --enable-scalers --enable-aspect --enable-detection-static > ../configure.log
        if ($LASTEXITCODE) { throw 'ScummVM project generation failed' }
        # The upstream generator uses the DLL LOCATION as a link library on
        # Windows. Link the imported target so CMake selects its import library.
        $content = Get-Content CMakeLists.txt -Raw
        $content = $content.Replace('get_target_property(SDL2_LIBRARIES SDL2::SDL2 LOCATION)', 'set(SDL2_LIBRARIES SDL2::SDL2)')
        $content = $content.Replace('target_link_libraries(scummvm ${SDL2_LIBRARIES} ${SCUMMVM_LIBS})', "list(REMOVE_ITEM SCUMMVM_LIBS SDL2::SDL2main)`n" + 'target_link_libraries(scummvm ${SDL2_LIBRARIES} ${SCUMMVM_LIBS})')
        $content += "`nif(MSVC)`n  set_property(TARGET scummvm PROPERTY WIN32_EXECUTABLE TRUE)`n  target_link_options(scummvm PRIVATE /MANIFEST:NO)`nendif()`n"
        Set-Content CMakeLists.txt $content
    } finally { Pop-Location }
    $sdl = Join-Path $root 'dependencies/SDL2-2.32.10/cmake'
    $fontIncludes = Join-Path $root 'dependencies/freetype-VER-2-13-3/include'
    $fontLibrary = Join-Path $root 'build/freetype/Release/freetype.lib'
    cmake -S build/generated -B build/scummvm -G 'Visual Studio 17 2022' -A x64 "-DSDL2_DIR=$sdl" "-DFREETYPE_INCLUDE_DIR_freetype2=$fontIncludes" "-DFREETYPE_INCLUDE_DIR_ft2build=$fontIncludes" "-DFREETYPE_LIBRARY_RELEASE=$fontLibrary"
    if ($LASTEXITCODE) { throw 'ScummVM configuration failed' }
    cmake --build build/scummvm --config Release --parallel 8
    if ($LASTEXITCODE) { throw 'ScummVM build failed' }
    Copy-Item dependencies/SDL2-2.32.10/lib/x64/SDL2.dll build/scummvm/Release/SDL2.dll -Force
    Copy-Item scummvm/dists/engine-data/fonts.dat build/scummvm/Release/fonts.dat -Force
    Copy-Item build/runtime-data/LiberationSans-Regular.ttf build/scummvm/Release/LiberationSans-Regular.ttf -Force
    Copy-Item build/runtime-data/Liberation-LICENSE.txt build/scummvm/Release/Liberation-LICENSE.txt -Force
} finally { Pop-Location }
