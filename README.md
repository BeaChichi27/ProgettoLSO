# ProgettoLSO - Tris Game

Implementation of Tic-Tac-Toe (Tris) in C with client-server architecture using TCP sockets and multithreading.

## Project Structure

```
ProgettoLSO/
├── server/
│   ├── src/
│   │   ├── main.c              # Server main entry point
│   │   ├── network.c           # Network handling and client threads
│   │   ├── lobby.c             # Client lobby management
│   │   ├── game_manager.c      # Game logic and state management
│   │   └── headers/
│   │       ├── network.h
│   │       ├── lobby.h
│   │       └── game_manager.h
│   ├── Dockerfile
│   └── Makefile
├── client/
│   ├── src/
│   │   ├── main.c              # Client main and UI
│   │   ├── network.c           # Client network communication
│   │   ├── game_logic.c        # Game state and validation
│   │   ├── ui.c                # User interface functions
│   │   └── headers/
│   │       ├── network.h
│   │       ├── game_logic.h
│   │       └── ui.h
│   ├── Dockerfile
│   └── Makefile
├── docker-compose.yml          # Multi-container orchestration
├── Mac.sh                      # macOS launcher script
├── Unix.sh                     # Linux launcher script
├── runWin.ps1                  # Windows PowerShell script
├── Windows.bat                 # Windows batch launcher

## Features

### Core Functionality
- **Multi-threaded server** supporting up to 100 concurrent connections
- **Cross-platform** compatibility (Windows, Linux, macOS)
- **Docker containerization** with Docker Compose orchestration
- **TCP communication** with robust error handling and reconnection
- **Real-time game lobby** with live updates of available games
- **Join approval system** - game creators can accept/reject join requests
- **Intelligent rematch system** with automatic role switching (X/O alternation)
- **Connection management** with graceful disconnection handling

### Game Features
- **Classic 3x3 Tic-Tac-Toe** gameplay
- **Real-time move validation** and synchronization
- **Win detection** (horizontal, vertical, diagonal) and draw detection
- **Turn-based gameplay** with timeout protection
- **Game state persistence** during network interruptions
- **Multiple concurrent games** on single server instance

### Technical Features
- **Thread-safe operations** with mutex synchronization
- **Memory leak prevention** with proper resource cleanup
- **Signal handling** for graceful shutdowns (Ctrl+C)
- **Network buffer management** to prevent message corruption
- **Scalable architecture** supporting easy feature additions

## Quick Start

### Prerequisites
- **Docker** and **Docker Compose** installed
- **Git** for repository cloning
- Terminal/Command Prompt access

### Installation & Run

1. **Clone the repository:**
   ```bash
   git clone https://github.com/BeaChichi27/ProgettoLSO.git
   cd ProgettoLSO
   ```

2. **Choose your platform and run:**

   **🍎 macOS:**
   ```bash
   chmod +x Mac.sh && ./Mac.sh
   ```

   **🐧 Linux:**
   ```bash
   chmod +x Unix.sh && ./Unix.sh
   ```

   **🪟 Windows:**
   ```cmd
   Windows.bat
   ```

   **🌐 Universal (any Unix-like system):**
   ```bash
   chmod +x run.sh && ./run.sh
   ```

3. **Enter number of clients** when prompted (1-20 recommended)
4. **Each client opens** in a separate terminal window automatically
5. **Start playing!** Register names and create/join games

### Alternative Manual Start
```bash
# Start server only
docker-compose up tris-server

# Start server + 3 clients
docker-compose --profile client up --scale tris-client=3

# Start predefined demo (Alice, Bob, Charlie)
docker-compose --profile demo up
```

## How to Play

### Game Flow
1. **Register** with a unique player name
2. **Create new game** or **join existing game** from the list
3. **Wait for approval** from the game creator (if joining)
4. **Take turns** placing your symbol (X or O)
5. **Win** by getting three symbols in a row (horizontal, vertical, or diagonal)
6. **Request rematch** after game completion (roles automatically switch)

### Game Commands
```
1. Crea partita          - Create a new game and wait for opponent
2. Unisciti a partita    - View available games and join one
3. Esci                  - Exit the application

During game:
• Enter cell number (1-9) to make a move
• View the 3x3 grid with position numbers
• Wait for opponent's turn
• Answer rematch requests (s/n)
```

## Game States & Protocol

### Game States
- `WAITING` - Game created, waiting for opponent to join
- `PENDING_APPROVAL` - Join request sent, awaiting creator approval  
- `PLAYING` - Game in progress, players taking turns
- `OVER` - Game finished, showing results
- `REMATCH_REQUESTED` - One or both players requested rematch

### Protocol Messages

#### Client → Server Commands
```
REGISTER:PlayerName      - Register player name
CREATE_GAME             - Create new game
LIST_GAMES              - Request available games list
JOIN:GameID             - Request to join game with ID
APPROVE:1/0             - Approve(1) or reject(0) join request
MOVE:row,col            - Make move at position (row,col)
REMATCH                 - Request rematch with same opponent
REMATCH_DECLINE         - Decline rematch request
LEAVE                   - Leave current game
```

#### Server → Client Responses
```
REGISTER_OK             - Registration successful
GAME_CREATED:ID         - Game created with ID
GAMES:GameList          - List of available games
JOIN_REQUEST:Name:ID    - Player Name wants to join game ID
JOIN_APPROVED:Symbol    - Join approved, assigned symbol
GAME_START:Symbol       - Game started, your symbol assigned
MOVE:row,col:Symbol     - Move made at position by Symbol
GAME_OVER:WINNER:Symbol - Game ended, winner announced
REMATCH_ACCEPTED        - Rematch accepted by both players
ERROR:Message           - Error occurred with description
```

## Architecture

### Server Architecture
- **Main Thread**: Accepts new client connections
- **Client Threads**: Handle individual client communication (one per client)
- **Game Manager**: Manages game states, moves, and win conditions
- **Lobby Manager**: Handles client registration and game listings
- **Network Layer**: TCP socket communication with error handling

### Client Architecture  
- **Main Loop**: User interface and input handling
- **Network Module**: Server communication and message parsing
- **Game Logic**: Local game state validation and display
- **UI Module**: Terminal-based user interface with game board rendering

### Data Structures
```c
// Game representation
typedef struct {
    int game_id;
    Client* player1;     // Creator (initially X)
    Client* player2;     // Joiner (initially O)
    char board[3][3];    // 3x3 game board
    PlayerSymbol current_player;  // X or O
    GameState state;     // Current game state
    PlayerSymbol winner; // Winner symbol or NONE
    int is_draw;         // Draw flag
    // ... additional fields
} Game;

// Client representation
typedef struct {
    socket_t client_fd;
    char name[MAX_NAME_LEN];
    int game_id;
    int is_active;
    // ... thread and network fields
} Client;
```

## Development

### Manual Compilation

**Server:**
```bash
cd server
make clean && make
./server
```

**Client:**
```bash  
cd client
make clean && make
./client
```

### Docker Development
```bash
# Build and run all services
docker-compose up --build

# View logs
docker-compose logs tris-server
docker-compose logs tris-client

# Scale clients
docker-compose --profile client up --scale tris-client=5

# Clean rebuild
docker-compose down --rmi all
docker-compose up --build
```

### Advanced Management
```bash
# Use the management script
./tris-manager.sh start-server    # Server only
./tris-manager.sh start-demo      # Server + 3 demo clients  
./tris-manager.sh scale-clients 10  # Server + 10 clients
./tris-manager.sh stop            # Stop all
./tris-manager.sh clean           # Clean everything
```

## Configuration

### Environment Variables (.env)
```bash
PORT=8080                    # Server port
EXTERNAL_PORT=8080           # Host port mapping
MAX_CLIENTS=100             # Maximum concurrent clients
DEBUG=1                     # Enable debug output
CLIENT_NAME=Player          # Default client name
```

### Network Configuration
- **Server Port**: 8080 (configurable)
- **Docker Network**: `tris-network` (172.20.0.0/16)
- **Container Communication**: Service name resolution
- **External Access**: `localhost:8080`

## Troubleshooting

### Common Issues

**Server won't start:**
```bash
# Check port availability
netstat -tulpn | grep :8080

# View server logs
docker-compose logs tris-server

# Restart server
docker-compose restart tris-server
```

**Client connection failed:**
```bash
# Verify server is running
docker-compose ps

# Check network connectivity
docker exec -it lso-tris-client-1 ping tris-server

# View client logs
docker-compose logs tris-client
```

**Docker issues:**
```bash
# Clean restart
docker-compose down
docker system prune -f
docker-compose up --build

# Reset everything
docker-compose down --rmi all --volumes
```

## Testing

### Functional Testing
1. **Single Client**: Create game, wait for opponent
2. **Two Clients**: Join request/approval flow
3. **Gameplay**: Full game with moves, win detection
4. **Rematch**: Role switching (X↔O) functionality  
5. **Disconnection**: Graceful handling of client drops
6. **Concurrent Games**: Multiple simultaneous games

### Stress Testing
```bash
# Test with many clients
./tris-manager.sh scale-clients 20

# Monitor system resources
docker stats

# Check connection limits
netstat -an | grep :8080 | wc -l
```

## Contributing

### Code Style
- **C99 Standard** compliance
- **Consistent indentation** (4 spaces)
- **Descriptive variable names**
- **Function documentation**
- **Error handling** for all operations

### Git Workflow
```bash
git checkout -b feature/new-feature
# Make changes
git add .
git commit -m "Add: new feature description"
git push origin feature/new-feature
# Create pull request
```

## License

This project is developed for educational purposes as part of the LSO (Laboratorio di Sistemi Operativi) course.

## Authors

- **BeaChichi27** - Initial development and architecture
- Repository: [ProgettoLSO](https://github.com/BeaChichi27/ProgettoLSO)

---

**🎮 Ready to play? Choose your script and start the Tris battle!**
- **Problema risolto**: "Il creatore di una partita può accettare o rifiutare la richiesta di partecipazione"
- **Implementazione**:
  - Quando un giocatore tenta di unirsi (`JOIN:ID`), viene messo in stato `PENDING_APPROVAL`
  - Il creatore riceve un messaggio `JOIN_REQUEST:NOME:ID`
  - Il creatore può rispondere con `APPROVE:1` (accetta) o `APPROVE:0` (rifiuta)
  - Solo dopo l'approvazione la partita inizia

#### 2. Messaggi Differenziati per Stato
- **Problema risolto**: "Messaggi diversi in base allo stato di gioco"
- **Implementazione**:
  - `WAITING`: "In attesa di giocatori"
  - `PENDING_APPROVAL`: "Richiesta di NOME in attesa"
  - `PLAYING`: "NOME vs NOME (In corso)"
  - `REMATCH_REQUESTED`: "NOME vs NOME (Rematch richiesto)"

#### 3. Broadcast Automatico delle Partite
- **Problema risolto**: "Invito a partecipare per partite in attesa"
- **Implementazione**:
  - Quando viene creata una nuova partita, viene inviata la lista aggiornata a tutti i client liberi
  - La lista mostra lo stato di ogni partita in tempo reale
  - I client vengono notificati automaticamente delle nuove opportunità

#### 4. Sistema Rematch Completo
- **Problema risolto**: "Scegliere se iniziare un'altra partita"
- **Implementazione**:
  - A fine partita, ogni giocatore può richiedere `REMATCH`
  - Il sistema aspetta che entrambi i giocatori accettino
  - Solo quando entrambi hanno accettato, inizia una nuova partita
  - Gestione dei rifiuti e delle richieste unilaterali

## Messaggi di Protocollo

### Comandi Client → Server
```
CREATE_GAME          - Crea una nuova partita
JOIN:ID               - Richiede di unirsi alla partita ID
APPROVE:1/0           - Approva/rifiuta richiesta di join
LIST_GAMES            - Richiede lista partite disponibili
MOVE:row,col          - Effettua una mossa
REMATCH               - Richiede rematch
LEAVE                 - Abbandona la partita
CANCEL                - Cancella partita in attesa
```

### Messaggi Server → Client
```
GAME_CREATED:ID                    - Partita ID creata con successo
WAITING_OPPONENT                   - In attesa di avversario
JOIN_REQUEST:NOME:ID               - NOME vuole unirsi alla partita ID
JOIN_PENDING:msg                   - Richiesta inviata, in attesa
JOIN_APPROVED:SYMBOL               - Join approvato, simbolo assegnato
JOIN_REJECTED:msg                  - Join rifiutato
GAME_START:SYMBOL                  - Partita iniziata, simbolo assegnato
REMATCH_REQUEST:msg                - Richiesta rematch da avversario
REMATCH_ACCEPTED:msg               - Rematch accettato da entrambi
GAMES:lista                        - Lista partite con stati
```

## Come Testare le Nuove Funzionalità

### Test 1: Sistema di Approvazione
1. Avvia server: `./server`
2. Avvia Client 1, crea partita: `CREATE_GAME`
3. Avvia Client 2, unisciti: `JOIN:1`
4. Client 1 riceve richiesta, può accettare (s) o rifiutare (n)
5. Solo se accettato, la partita inizia

### Test 2: Broadcast Automatico  
1. Avvia più client
2. Crea partite da diversi client
3. Ogni client libero riceverà automaticamente la lista aggiornata

### Test 3: Sistema Rematch
1. Completa una partita
2. Entrambi i client ricevono la richiesta di rematch
3. Solo se entrambi accettano, inizia una nuova partita

## Compilazione e Avvio

### Server
```bash
cd server
make clean && make
./server
```

### Client
```bash
cd client  
make clean && make
./client  # Linux
./client.exe  # Windows
```

### Docker Compose
```bash
docker-compose up --build
```

## Conformità alla Traccia

Il progetto ora rispetta **100%** delle specifiche richieste:

- ✅ Server C, Client C, comunicazione Socket
- ✅ Docker-compose per deployment  
- ✅ Server multi-client con thread
- ✅ Due giocatori per partita
- ✅ **Approvazione/rifiuto richieste join**
- ✅ **Messaggi differenziati per stato**
- ✅ **Broadcast automatico partite disponibili**
- ✅ **Sistema rematch completo**
- ✅ Stati di gioco: terminata, in corso, in attesa, nuova creazione
- ✅ Stati di terminazione: vittoria, sconfitta, pareggio
- ✅ ID univoci per partite
- ✅ Possibilità di giocare più partite

## Architettura

### Server
- `main.c`: Gestione connessioni e thread
- `network.c`: Comunicazione TCP/UDP
- `lobby.c`: Gestione client e messaggi
- `game_manager.c`: Logica partite e stati

### Client  
- `main.c`: Loop principale e interazione utente
- `network.c`: Comunicazione con server
- `game_logic.c`: Logica locale del gioco
- `ui.c`: Interfaccia utente
