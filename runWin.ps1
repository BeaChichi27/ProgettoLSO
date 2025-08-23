# Script PowerShell per avvio Tris con Docker Compose

# Ottieni la directory dello script
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

# Verifica che docker-compose.yml esista
if (-not (Test-Path "docker-compose.yml")) {
    Write-Host "❌ File docker-compose.yml non trovato nella directory corrente" -ForegroundColor Red
    Read-Host "Premere Enter per uscire"
    exit 1
}

# Chiedi all'utente il numero di client
Write-Host "=== TRIS GAME LAUNCHER ===" -ForegroundColor Cyan
Write-Host ""
do {
    $n_client = Read-Host "Inserisci il numero di client da avviare (1-10)"
    if (-not ($n_client -as [int]) -or $n_client -lt 1 -or $n_client -gt 10) {
        Write-Host "❌ Inserire un numero valido tra 1 e 10" -ForegroundColor Red
    }
} while (-not ($n_client -as [int]) -or $n_client -lt 1 -or $n_client -gt 10)

$projectName = "lso"
$clientName = "tris-client"
$serverName = "tris-server"

Write-Host ""
Write-Host "🧹 Pulizia container esistenti..." -ForegroundColor Yellow

# Ferma tutti i servizi esistenti
docker-compose --profile demo --profile client down 2>$null | Out-Null

Write-Host "🚀 Avvio del server..." -ForegroundColor Green

# Avvia il server in una nuova finestra PowerShell
$serverScript = @"
Set-Location '$scriptDir'
Write-Host '🚀 Avvio server Tris...' -ForegroundColor Green
Write-Host ''
docker-compose up tris-server
Write-Host ''
Write-Host '⚠️  Server terminato. Premere un tasto per chiudere...' -ForegroundColor Yellow
`$null = `$Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
"@

Start-Process powershell -ArgumentList "-NoExit", "-Command", $serverScript

# Attendi che il server sia pronto
Write-Host "⏳ Attendo che il server sia pronto..." -ForegroundColor Yellow
Start-Sleep -Seconds 8

# Verifica che il server sia attivo
$serverRunning = $false
$maxRetries = 10
$retries = 0

while (-not $serverRunning -and $retries -lt $maxRetries) {
    $containers = docker-compose ps tris-server 2>$null
    if ($containers -match "Up") {
        $serverRunning = $true
    } else {
        Start-Sleep -Seconds 2
        $retries++
    }
}

if (-not $serverRunning) {
    Write-Host "❌ Il server non si è avviato correttamente" -ForegroundColor Red
    Read-Host "Premere Enter per uscire"
    exit 1
}

Write-Host "🎮 Avvio di $n_client client..." -ForegroundColor Green

# Avvia i client usando scaling
docker-compose --profile client up -d --scale tris-client=$n_client 2>$null | Out-Null

# Attendi che tutti i container client siano avviati
Write-Host "⏳ Attendo che i client siano pronti..." -ForegroundColor Yellow
$allContainersRunning = $false
$maxWait = 30
$waited = 0

while (-not $allContainersRunning -and $waited -lt $maxWait) {
    $containers = docker ps --format "{{.Names}}" | Where-Object { $_ -match "tris-client" }
    
    if ($containers.Count -ge $n_client) {
        $allContainersRunning = $true
    } else {
        Start-Sleep -Seconds 2
        $waited += 2
    }
}

if (-not $allContainersRunning) {
    Write-Host "❌ Non tutti i client si sono avviati" -ForegroundColor Red
    Read-Host "Premere Enter per uscire"
    exit 1
}

# Ottieni la lista dei container client
$clientContainers = docker ps --format "{{.Names}}" | Where-Object { $_ -match "tris-client" } | Sort-Object

if ($clientContainers.Count -eq 0) {
    Write-Host "❌ Nessun container client trovato" -ForegroundColor Red
    Read-Host "Premere Enter per uscire"
    exit 1
}

Write-Host "🖥️  Apertura finestre per ogni client..." -ForegroundColor Green

# Avvia una finestra PowerShell per ogni client
$clientContainers | ForEach-Object {
    $containerName = $_
    $clientScript = @"
Set-Location '$scriptDir'
Write-Host '🎮 Connessione al client: $containerName' -ForegroundColor Cyan
Write-Host ''
docker attach $containerName
Write-Host ''
Write-Host '⚠️  Client disconnesso. Premere un tasto per chiudere...' -ForegroundColor Yellow
`$null = `$Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
"@
    
    Start-Process powershell -ArgumentList "-NoExit", "-Command", $clientScript
    Start-Sleep -Seconds 1
}

Write-Host ""
Write-Host "✅ Tutto pronto!" -ForegroundColor Green
Write-Host "📊 Server: http://localhost:8080" -ForegroundColor Cyan
Write-Host "🎮 Client attivi: $($clientContainers.Count)" -ForegroundColor Cyan
Write-Host ""
Write-Host "💡 Per fermare tutto:" -ForegroundColor Yellow
Write-Host "   docker-compose --profile client --profile demo down" -ForegroundColor White
Write-Host ""
Read-Host "Premere Enter per continuare"
