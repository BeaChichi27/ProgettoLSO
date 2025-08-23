@echo off
setlocal enabledelayedexpansion

:: Imposta il titolo della finestra
title Tris Game Launcher

:: Ottieni la directory dello script
set "scriptDir=%~dp0"
cd /d "%scriptDir%"

:: Verifica che docker-compose.yml esista
if not exist "docker-compose.yml" (
    echo.
    echo ❌ File docker-compose.yml non trovato nella directory corrente
    echo.
    pause
    exit /b 1
)

echo.
echo === TRIS GAME LAUNCHER ===
echo.

:: Percorso completo allo script PowerShell
set "ps1File=%scriptDir%runWin.ps1"

:: Verifica che lo script PowerShell esista
if not exist "%ps1File%" (
    echo ❌ File runWin.ps1 non trovato
    echo.
    pause
    exit /b 1
)

:: Sblocca lo script se è stato bloccato da Windows
echo 🔓 Preparazione script PowerShell...
powershell -Command "try { Unblock-File -Path '%ps1File%' -ErrorAction SilentlyContinue } catch { }" >nul 2>&1

:: Verifica che Docker sia in esecuzione
echo 🐳 Verifica Docker...
docker version >nul 2>&1
if errorlevel 1 (
    echo ❌ Docker non è in esecuzione o non è installato
    echo.
    echo 💡 Assicurati che Docker Desktop sia avviato
    echo.
    pause
    exit /b 1
)

:: Verifica che docker-compose sia disponibile
docker-compose version >nul 2>&1
if errorlevel 1 (
    echo ❌ docker-compose non disponibile
    echo.
    pause
    exit /b 1
)

echo ✅ Docker è pronto
echo.

:: Avvia lo script PowerShell con ExecutionPolicy Bypass
echo 🚀 Avvio interfaccia PowerShell...
echo.
powershell -NoExit -ExecutionPolicy Bypass -File "%ps1File%"

:: Se arriviamo qui, lo script PowerShell è terminato
echo.
echo 🔚 Script terminato
echo.
pause
