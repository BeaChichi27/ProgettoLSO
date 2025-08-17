# Progetto Tris Client-Server

## Panoramica
Implementazione completa di un gioco del Tris client-server che rispetta tutte le specifiche richieste dalla traccia del progetto LSO.

## Funzionalità Implementate

### ✅ Requisiti Base
- **Server multi-client** sviluppato in C
- **Client** sviluppato in C  
- **Comunicazione tramite Socket** (TCP + UDP)
- **Docker-compose** per deployment
- **Due giocatori per partita** con gestione delle sessioni
- **Stati di gioco**: WAITING, PENDING_APPROVAL, PLAYING, OVER, REMATCH_REQUESTED
- **ID univoci** per ogni partita
- **Logica completa del Tris** (3x3, controllo vittorie/pareggi)

### ✅ Funzionalità Avanzate

#### 1. Sistema di Approvazione Join
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
