#!/bin/bash

# Percorso alla cartella del progetto
PROGETTO="$(dirname "$(realpath "$0")")"

# Nome dei servizi definiti in docker-compose
SERVER_NAME="tris-server"
CLIENT_NAME="tris-client"
PROJECT_NAME="lso"  # Nome del progetto docker-compose

# Vai nella directory del progetto
cd "$PROGETTO" || {
  echo "❌ Impossibile trovare la cartella del progetto: $PROGETTO"
  exit 1
}

# Verifica che docker-compose.yml esista
if [ ! -f "docker-compose.yml" ]; then
  echo "❌ File docker-compose.yml non trovato nella directory corrente"
  exit 1
fi

# Chiedi il numero di client
read -rp "Inserisci il numero di client da avviare: " N

# Controllo input
if ! [[ "$N" =~ ^[0-9]+$ ]] || [ "$N" -lt 1 ]; then
  echo "❌ Inserire un numero intero valido maggiore di 0."
  exit 1
fi

echo "🧹 Pulizia container esistenti..."

# Ferma tutti i servizi esistenti
docker-compose --profile demo --profile client down >/dev/null 2>&1

echo "🚀 Avvio del server..."
# Avvia solo il server prima
docker-compose up -d tris-server

# Attendi che il server sia pronto
echo "⏳ Attendo che il server sia pronto..."
sleep 5

# Controlla che il server sia effettivamente avviato
if ! docker-compose ps tris-server | grep -q "Up"; then
  echo "❌ Errore nell'avvio del server"
  exit 1
fi

echo "🎮 Avvio di $N client..."

# Avvia i client usando scaling
docker-compose --profile client up -d --scale tris-client="$N"

# Attendi che i client siano pronti
sleep 3

# Trova tutti i container client attivi
CLIENT_CONTAINERS=$(docker ps --format "{{.Names}}" | grep "${PROJECT_NAME}.*${CLIENT_NAME}" | sort)

if [ -z "$CLIENT_CONTAINERS" ]; then
  echo "❌ Nessun container client trovato"
  exit 1
fi

# Apri una finestra Terminal per ogni client
echo "🖥️  Apertura finestre Terminal per ogni client..."
for CONTAINER in $CLIENT_CONTAINERS; do
  osascript <<EOF
tell application "Terminal"
  set newTab to do script "echo '🎮 Client: $CONTAINER'; echo ''; docker attach $CONTAINER"
  set custom title of newTab to "Tris Client - $CONTAINER"
  activate
end tell
EOF
  sleep 1
done

echo "✅ Tutto pronto!"
echo "📊 Server: http://localhost:8080"
echo "🎮 Client attivi: $(echo "$CLIENT_CONTAINERS" | wc -l)"
echo ""
echo "💡 Per fermare tutto: docker-compose --profile client --profile demo down"
