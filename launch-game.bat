@echo off
cd /d "%~dp0"
set "LB2_GAME_EXE=build\pc-play\legobatman2.exe"
if not exist "%LB2_GAME_EXE%" set "LB2_GAME_EXE=build\host\legobatman2.exe"
if not exist "%LB2_GAME_EXE%" (
    echo Game executable not found: build\host\legobatman2.exe
    pause
    exit /b 1
)
if not exist "game\Default.xex" (
    echo Game data not found: game\Default.xex
    pause
    exit /b 1
)
"%LB2_GAME_EXE%"
if errorlevel 1 (
    echo.
    echo Game exited with an error. Check the logs folder next to the game executable.
    pause
)
