# 🎮 Tris Game - Scripts di Avvio

Questo repository contiene scripts multi-piattaforma per avviare facilmente il gioco del Tris con Docker Compose.

## 📋 Prerequisiti

- **Docker** installato e in esecuzione
- **Docker Compose** installato
- Sistema operativo supportato: Windows, macOS, Linux

## 🚀 Scripts Disponibili

### 🖥️ **run.sh** - Script Universale
Script principale che funziona su tutti i sistemi Unix-like:
```bash
./run.sh
```
**Caratteristiche:**
- ✅ Verifica automatica dei prerequisiti
- ✅ Interfaccia utente intuitiva
- ✅ Gestione errori avanzata
- ✅ Status del sistema in tempo reale

### 🍎 **Mac.sh** - Specifico per macOS
Ottimizzato per macOS con apertura automatica delle finestre Terminal:
```bash
./Mac.sh
```
**Caratteristiche:**
- 🖥️ Apre automaticamente una finestra Terminal per ogni client
- 🎯 Titoli personalizzati per ogni finestra
- 🍎 Integrazione nativa con macOS

### 🐧 **Unix.sh** - Linux/Unix
Supporta diversi emulatori di terminale Linux:
```bash
./Unix.sh
```
**Terminali supportati:**
- GNOME Terminal
- Konsole (KDE)
- XFCE4 Terminal
- XTerm

### 🪟 **Windows.bat** + **runWin.ps1** - Windows
Per sistemi Windows:
```cmd
Windows.bat
```
**Caratteristiche:**
- 🔓 Sblocco automatico script PowerShell
- 🪟 Finestre PowerShell multiple
- 🛡️ Gestione ExecutionPolicy

## 🎯 Utilizzo

1. **Avvia lo script appropriato per il tuo sistema:**
   ```bash
   # Linux/macOS
   ./run.sh
   
   # macOS (con finestre automatiche)
   ./Mac.sh
   
   # Windows
   Windows.bat
   ```

2. **Inserisci il numero di client** quando richiesto (1-20)

3. **Attendi che il sistema si avvii:**
   - Il server si avvia per primo
   - I client vengono avviati in parallelo
   - Le finestre si aprono automaticamente (Mac/Windows)

## 🔧 Comandi Manuali

### Avvio Manuale
```bash
# Solo server
docker-compose up tris-server

# Server + 3 client
docker-compose --profile client up --scale tris-client=3

# Demo con client predefiniti
docker-compose --profile demo up
```

### Connessione ai Client
```bash
# Lista client attivi
docker ps --format "{{.Names}}" | grep tris-client

# Connetti a un client specifico
docker attach lso-tris-client-1

# Disconnetti senza terminare: Ctrl+P, Ctrl+Q
```

### Arresto
```bash
# Ferma tutto
docker-compose --profile client --profile demo down

# Rimuovi anche i volumi
docker-compose --profile client --profile demo down -v
```

## 🐛 Risoluzione Problemi

### Server non si avvia
```bash
# Verifica status
docker-compose ps

# Visualizza logs
docker-compose logs tris-server

# Riavvia
docker-compose restart tris-server
```

### Client non si connette
```bash
# Verifica network
docker network ls | grep lso

# Verifica connettività
docker exec -it lso-tris-client-1 ping tris-server
```

### Pulizia completa
```bash
# Rimuovi tutto
docker-compose down --rmi all --volumes
docker system prune -f
```

## 📊 Architettura

```
┌─────────────────┐    ┌─────────────────┐
│   tris-server   │    │  tris-client-1  │
│   (container)   │◄──►│   (container)   │
│  Port: 8080     │    │                 │
└─────────────────┘    └─────────────────┘
         ▲                       ▲
         │              ┌─────────────────┐
         │              │  tris-client-N  │
         └──────────────►│   (container)   │
                        │                 │
                        └─────────────────┘
```

## 🌐 Network

- **Network**: `tris-network` (172.20.0.0/16)
- **Server**: `tris-server:8080`
- **Porta esterna**: `localhost:8080`

## 📁 File di Configurazione

- **docker-compose.yml**: Configurazione servizi
- **.env**: Variabili d'ambiente
- **.gitignore**: File da ignorare
- **tris-manager.sh**: Script di gestione avanzato

## 🎮 Come Giocare

1. Il server accetta connessioni multiple
2. Ogni client può creare o unirsi a partite
3. Le partite supportano il rematch
4. Alternanza automatica dei simboli (X/O)

## 📝 Note

- Limite massimo consigliato: 20 client simultanei
- Il server gestisce fino a 100 connessioni (configurabile)
- I container sono isolati e comunicano via Docker network
- Logs disponibili in `./logs/` (se configurato)
