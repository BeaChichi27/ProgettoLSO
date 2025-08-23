#!/bin/bash

# Percorso alla directory del progetto
WORK_DIR="$(dirname "$(realpath "$0")")"

# Nome dei servizi
SERVER_NAME="tris-server"
CLIENT_NAME="tris-client"
PROJECT_NAME="lso"

# Entra nella directory del progetto
cd "$WORK_DIR" || {
    echo "❌ Errore: Impossibile entrare nella directory del progetto"
    exit 1
}

# Verifica che docker-compose.yml esista
if [ ! -f "docker-compose.yml" ]; then
    echo "❌ File docker-compose.yml non trovato"
    exit 1
fi

# Rileva il tipo di terminale disponibile
detect_terminal() {
    if command -v gnome-terminal >/dev/null 2>&1; then
        echo "gnome-terminal"
    elif command -v konsole >/dev/null 2>&1; then
        echo "konsole"
    elif command -v xfce4-terminal >/dev/null 2>&1; then
        echo "xfce4-terminal"
    elif command -v xterm >/dev/null 2>&1; then
        echo "xterm"
    else
        echo "none"
    fi
}

TERMINAL=$(detect_terminal)

if [ "$TERMINAL" = "none" ]; then
    echo "❌ Nessun emulatore di terminale supportato trovato"
    exit 1
fi

# Chiedi il numero di client
echo "Inserisci il numero di client da avviare:"
read -r n_client

# Verifica input
if ! [[ "$n_client" =~ ^[0-9]+$ ]] || [ "$n_client" -lt 1 ]; then
    echo "❌ Inserire un numero intero valido maggiore di 0"
    exit 1
fi

echo "🧹 Pulizia container esistenti..."

# Ferma tutti i servizi esistenti
docker-compose --profile demo --profile client down >/dev/null 2>&1

echo "🚀 Avvio del server..."

# Avvia il server in una nuova finestra terminale
case "$TERMINAL" in
    "gnome-terminal")
        gnome-terminal --title="Tris Server" -- bash -c "
            cd '$WORK_DIR'
            echo '🚀 Avvio server Tris...'
            docker-compose up tris-server
            echo ''
            echo '⚠️  Server terminato. Premere Enter per chiudere...'
            read
        " &
        ;;
    "konsole")
        konsole --title="Tris Server" -e bash -c "
            cd '$WORK_DIR'
            echo '🚀 Avvio server Tris...'
            docker-compose up tris-server
            echo ''
            echo '⚠️  Server terminato. Premere Enter per chiudere...'
            read
        " &
        ;;
    "xfce4-terminal")
        xfce4-terminal --title="Tris Server" -e bash -c "
            cd '$WORK_DIR'
            echo '🚀 Avvio server Tris...'
            docker-compose up tris-server
            echo ''
            echo '⚠️  Server terminato. Premere Enter per chiudere...'
            read
        " &
        ;;
    "xterm")
        xterm -title "Tris Server" -e bash -c "
            cd '$WORK_DIR'
            echo '🚀 Avvio server Tris...'
            docker-compose up tris-server
            echo ''
            echo '⚠️  Server terminato. Premere Enter per chiudere...'
            read
        " &
        ;;
esac

# Attendi che il server sia pronto
echo "⏳ Attendo che il server sia pronto..."
sleep 8

# Verifica che il server sia attivo
if ! docker-compose ps tris-server 2>/dev/null | grep -q "Up"; then
    echo "❌ Il server non si è avviato correttamente"
    exit 1
fi

echo "🎮 Avvio di $n_client client..."

# Avvia i client con scaling
docker-compose --profile client up -d --scale tris-client="$n_client" >/dev/null

# Attendi che i client siano pronti
sleep 3

# Trova tutti i container client
CLIENT_CONTAINERS=$(docker ps --format "{{.Names}}" | grep "tris-client" | sort)

if [ -z "$CLIENT_CONTAINERS" ]; then
    echo "❌ Nessun container client trovato"
    exit 1
fi

# Apri una finestra terminale per ogni client
echo "🖥️  Apertura terminali per ogni client..."
for container in $CLIENT_CONTAINERS; do
    case "$TERMINAL" in
        "gnome-terminal")
            gnome-terminal --title="Client - $container" -- bash -c "
                echo '🎮 Connessione al client: $container'
                echo ''
                docker attach '$container'
                echo ''
                echo '⚠️  Client disconnesso. Premere Enter per chiudere...'
                read
            " &
            ;;
        "konsole")
            konsole --title="Client - $container" -e bash -c "
                echo '🎮 Connessione al client: $container'
                echo ''
                docker attach '$container'
                echo ''
                echo '⚠️  Client disconnesso. Premere Enter per chiudere...'
                read
            " &
            ;;
        "xfce4-terminal")
            xfce4-terminal --title="Client - $container" -e bash -c "
                echo '🎮 Connessione al client: $container'
                echo ''
                docker attach '$container'
                echo ''
                echo '⚠️  Client disconnesso. Premere Enter per chiudere...'
                read
            " &
            ;;
        "xterm")
            xterm -title "Client - $container" -e bash -c "
                echo '🎮 Connessione al client: $container'
                echo ''
                docker attach '$container'
                echo ''
                echo '⚠️  Client disconnesso. Premere Enter per chiudere...'
                read
            " &
            ;;
    esac
    sleep 1
done

echo "✅ Tutto pronto!"
echo "📊 Server: http://localhost:8080"
echo "🎮 Client attivi: $(echo "$CLIENT_CONTAINERS" | wc -l)"
echo ""
echo "💡 Per fermare tutto:"
echo "   docker-compose --profile client --profile demo down"
