#!/bin/bash

# Script universale per avvio Tris Game
# Supporta Linux, macOS e Windows (tramite Git Bash/WSL)

set -e

# Ottieni la directory dello script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Funzioni di utilità
show_banner() {
    echo "╔══════════════════════════════════════╗"
    echo "║          🎮 TRIS GAME LAUNCHER 🎮    ║"
    echo "║              Docker Edition          ║"
    echo "╚══════════════════════════════════════╝"
    echo ""
}

check_requirements() {
    echo "🔍 Verifica requisiti..."
    
    # Verifica Docker
    if ! command -v docker &> /dev/null; then
        echo "❌ Docker non trovato. Installare Docker prima di continuare."
        exit 1
    fi
    
    # Verifica docker-compose
    if ! command -v docker-compose &> /dev/null; then
        echo "❌ docker-compose non trovato. Installare docker-compose prima di continuare."
        exit 1
    fi
    
    # Verifica che Docker sia in esecuzione
    if ! docker info &> /dev/null; then
        echo "❌ Docker non è in esecuzione. Avviare Docker e riprovare."
        exit 1
    fi
    
    # Verifica docker-compose.yml
    if [ ! -f "docker-compose.yml" ]; then
        echo "❌ File docker-compose.yml non trovato nella directory corrente."
        exit 1
    fi
    
    echo "✅ Tutti i requisiti sono soddisfatti"
}

get_client_count() {
    while true; do
        read -p "Inserisci il numero di client da avviare (1-20): " n_client
        if [[ "$n_client" =~ ^[0-9]+$ ]] && [ "$n_client" -ge 1 ] && [ "$n_client" -le 20 ]; then
            echo "$n_client"
            return
        else
            echo "❌ Inserire un numero valido tra 1 e 20"
        fi
    done
}

cleanup_containers() {
    echo "🧹 Pulizia container esistenti..."
    docker-compose --profile demo --profile client down &>/dev/null || true
    echo "✅ Pulizia completata"
}

start_server() {
    echo "🚀 Avvio del server..."
    docker-compose up -d tris-server
    
    echo "⏳ Attendo che il server sia pronto..."
    local retries=0
    local max_retries=30
    
    while [ $retries -lt $max_retries ]; do
        if docker-compose ps tris-server | grep -q "Up"; then
            echo "✅ Server avviato con successo"
            return 0
        fi
        sleep 2
        retries=$((retries + 1))
    done
    
    echo "❌ Timeout: il server non si è avviato entro 60 secondi"
    return 1
}

start_clients() {
    local n_client=$1
    echo "🎮 Avvio di $n_client client..."
    
    docker-compose --profile client up -d --scale tris-client="$n_client"
    
    echo "⏳ Attendo che i client siano pronti..."
    sleep 5
    
    # Conta i client attivi
    local active_clients=$(docker ps --format "{{.Names}}" | grep -c "tris-client" || echo "0")
    
    if [ "$active_clients" -ge "$n_client" ]; then
        echo "✅ $active_clients client avviati con successo"
        return 0
    else
        echo "⚠️  Solo $active_clients client su $n_client richiesti sono attivi"
        return 1
    fi
}

show_status() {
    echo ""
    echo "📊 STATO SISTEMA:"
    echo "═══════════════════"
    
    # Status server
    if docker-compose ps tris-server | grep -q "Up"; then
        echo "🟢 Server: ATTIVO (http://localhost:8080)"
    else
        echo "🔴 Server: NON ATTIVO"
    fi
    
    # Status client
    local client_count=$(docker ps --format "{{.Names}}" | grep -c "tris-client" || echo "0")
    echo "🎮 Client attivi: $client_count"
    
    if [ "$client_count" -gt 0 ]; then
        echo ""
        echo "💡 Per connettersi manualmente ai client:"
        docker ps --format "{{.Names}}" | grep "tris-client" | while read container; do
            echo "   docker attach $container"
        done
    fi
    
    echo ""
    echo "🛑 Per fermare tutto:"
    echo "   docker-compose --profile client --profile demo down"
    echo ""
}

# Main execution
main() {
    show_banner
    check_requirements
    
    echo ""
    n_client=$(get_client_count)
    
    echo ""
    cleanup_containers
    
    echo ""
    if ! start_server; then
        echo "❌ Errore nell'avvio del server"
        exit 1
    fi
    
    echo ""
    if ! start_clients "$n_client"; then
        echo "⚠️  Alcuni client potrebbero non essere disponibili"
    fi
    
    show_status
    
    echo "🎉 Sistema pronto per giocare!"
    echo ""
    echo "💡 Suggerimenti:"
    echo "   • Apri terminali separati per ogni client"
    echo "   • Usa 'docker attach <container-name>' per connetterti"
    echo "   • Usa Ctrl+P, Ctrl+Q per disconnetterti senza terminare il client"
    echo ""
}

# Esegui se chiamato direttamente
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
