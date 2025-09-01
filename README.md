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
│   │       ├── network.h       # Network function declarations
│   │       ├── lobby.h         # Lobby management declarations
│   │       └── game_manager.h  # Game management declarations
│   ├── .dockerignore
│   ├── Dockerfile              # Server container configuration
│   ├── Makefile                # Server build configuration
│   └── server                  # Compiled server executable
├── client/
│   ├── src/
│   │   ├── main.c              # Client main and UI
│   │   ├── network.c           # Client network communication
│   │   ├── game_logic.c        # Game state and validation
│   │   ├── ui.c                # User interface functions
│   │   └── headers/
│   │       ├── network.h       # Network function declarations
│   │       ├── game_logic.h    # Game logic declarations
│   │       └── ui.h            # UI function declarations
│   ├── .dockerignore
│   ├── Dockerfile              # Client container configuration
│   ├── Makefile                # Client build configuration
│   └── client                  # Compiled client executable
├── .env                        # Environment variables
├── .gitignore                  # Git ignore rules
├── .vscode/                    # VS Code configuration
├── docker-compose.yml          # Multi-container orchestration
├── Mac.sh                      # macOS launcher script
├── Unix.sh                     # Linux launcher script
├── run.sh                      # Universal Unix launcher script
├── runWin.ps1                  # Windows PowerShell script
├── Windows.bat                 # Windows batch launcher
└── README.md                   # This documentation file
```

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

## Configuration

### Environment Variables (.env)
```properties
# Configurazione server
PORT=8080                       # Server port
EXTERNAL_PORT=8080              # Host port mapping
MAX_CLIENTS=100                 # Maximum concurrent clients
DEBUG=1                         # Enable debug output

# Configurazioni client
CLIENT_NAME=Player              # Default client name

# Compose project name
COMPOSE_PROJECT_NAME=tris-game  # Docker Compose project name
```

### Network Configuration
- **Server Port**: 8080 (configurable via .env)
- **Docker Network**: Managed by Docker Compose
- **Container Communication**: Service name resolution
- **External Access**: `localhost:8080`

## Authors

- **BeaChichi27** - Initial development and architecture
- Repository: [ProgettoLSO](https://github.com/BeaChichi27/ProgettoLSO)

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

## Architecture

### Server Components
- **`main.c`** - Server entry point, connection handling, signal management
- **`network.c`** - TCP/UDP communication, client threads, protocol handling  
- **`lobby.c`** - Client registration, game listing, message routing
- **`game_manager.c`** - Game logic, state management, move validation

### Client Components  
- **`main.c`** - Client entry point, user interface, game flow
- **`network.c`** - Server communication, connection management
- **`game_logic.c`** - Local game state, move validation, win detection
- **`ui.c`** - Terminal interface, board display, user input

## Code Documentation

All source files (.c) include comprehensive **Javadoc-style documentation**: