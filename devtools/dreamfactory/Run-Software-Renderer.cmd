@echo off
setlocal
set "SDL_RENDER_DRIVER=software"
start "" "%~dp0scummvm.exe" --gfx-mode=surfacesdl %*
