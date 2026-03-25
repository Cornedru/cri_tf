# Résumé Technique - Serveur IRC ft_irc

## Problème initial

Les fichiers d'en-tête dans `inc/` contenaient uniquement des forward declarations:
```cpp
class Client;
class Channel;
class Message;
```

Cela cause des erreurs de compilation car le code dans `Server.cpp` utilise ces classes complètement (pour `delete`, accès aux méthodes, etc.).

## Solution appliquée

### 1. Inclusion des headers complets dans Server.hpp

```cpp
// AVANT (incorrect):
class Client;
class Channel;

// APRÈS (correct):
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"
#include "CommandHandler.hpp"
```

### 2. Implémentation minimale des classes

Créé les fichiers `.hpp` et `.cpp` pour:
- **Client** - Gestion du client connecté (fd, buffer, état)
- **Channel** - Gestion des canaux IRC
- **Message** - Parsing des messages IRC
- **CommandHandler** - Traitement des commandes

---

## Concepts fondamentaux TCP/Sockets

### 1. Création du socket serveur

```cpp
_sockfd = socket(AF_INET, SOCK_STREAM, 0);  // TCP IPv4
```

- `AF_INET` = IPv4
- `SOCK_STREAM` = TCP (fiable, orienté connexion)
- `SOCK_DGRAM` = UDP (non fiable, sans connexion)

### 2. Configuration du socket

```cpp
// Permet de réutiliser le port rapidement après restart
int opt = 1;
setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

### 3. Binding - Attachement à une adresse

```cpp
struct sockaddr_in addr;
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = INADDR_ANY;  // Toutes les interfaces
addr.sin_port = htons(_port);       // Conversion port → réseau (big-endian)
bind(_sockfd, (struct sockaddr*)&addr, sizeof(addr));
```

### 4. Mode écoute

```cpp
listen(_sockfd, 10);  // Backlog = 10 connexions en attente max
```

### 5. Mode non-bloquant (O_NONBLOCK)

```cpp
int flags = fcntl(_sockfd, F_GETFL, 0);
fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);
```

**Pourquoi?** Sans O_NONBLOCK, `accept()`, `recv()`, `send()` peuvent bloquer le processus. En mode non-bloquant:
- `accept()` retourne -1 si pas de connexion en attente (errno = EAGAIN)
- `recv()` retourne -1 si pas de données (errno = EAGAIN)
- `send()` retourne -1 si buffer plein (errno = EAGAIN)

### 6. Boucle poll() - Multiplexage I/O

```cpp
struct pollfd pfd;
pfd.fd = _sockfd;
pfd.events = POLLIN;  // Surveiller les données à lire
_pollfds.push_back(pfd);

// Dans la boucle:
int n = poll(&_pollfds[0], _pollfds.size(), -1);  // -1 = attente infinie
```

`poll()` surveille plusieurs fd simultanément. Returns quand:
- Des données sont disponibles (POLLIN)
- Une connexion arrive (sur socket d'écoute)
- Une erreur survient

### 7. Acceptation de connexion

```cpp
int clientFd = accept(_sockfd, (struct sockaddr*)&clientAddr, &clientLen);
```

Retourne un **nouveau socket** pour communiquer avec le client. Le socket d'écoute reste ouvert pour d'autres connexions.

### 8. Réception des données (recv)

```cpp
char buffer[1024];
int ret = recv(fd, buffer, sizeof(buffer) - 1, 0);

if (ret <= 0) {
    // ret == 0 : client a fermé la connexion (EOF)
    // ret < 0 && errno == EAGAIN : pas de données, retry
    // ret < 0 && autre erreur : problème réseau
}
```

### 9. Buffer de réception - Problème critique

Les données TCP arrivent **en fragments**. Un message peut être split en plusieurs recv():

```
Client envoie: "PASS\r\nNICK alice\r\n"
                        ↓
Fragment 1: "PAS"       →rien→ buffer: "PAS"
Fragment 2: "S\r\nNI"   →rien→ buffer: "PASS\r\nNI"
Fragment 3: "CK\r\n"   →traite→ buffer: "" (traite "PASS")
```

Solution - Buffer d'accumulation:

```cpp
void Client::appendToBuffer(const std::string& data) {
    _buffer += data;  // Accumule les données partielles
}

std::vector<std::string> Client::extractMessages() {
    std::vector<std::string> messages;
    size_t pos;
    // Cherche \r\n (fin de message IRC)
    while ((pos = _buffer.find("\r\n")) != std::string::npos) {
        messages.push_back(_buffer.substr(0, pos));
        _buffer.erase(0, pos + 2);  // Supprime le message traité
    }
    return messages;
}
```

### 10. Envoi des données (send)

```cpp
void Client::sendMsg(const std::string& msg) {
    send(_fd, msg.c_str(), msg.size(), 0);
}
```

Important: Ajouter `\r\n` à la fin de chaque message IRC!

---

## Architecture du serveur

```
┌─────────────────────────────────────────────────────┐
│                   Server                             │
│  - Socket d'écoute (port)                           │
│  - Map<fd, Client*>                                 │
│  - Map<nom, Channel*>                               │
│  - Vecteur<pollfd>                                  │
└─────────────────────────────────────────────────────┘
          │                    │
          ▼                    ▼
   acceptClient()        handleClientData(fd)
          │                    │
          ▼                    ▼
   Création Client      recv() → Buffer → parse()
          │                    │
          ▼                    ▼
   Ajout à poll()       Message::parse()
                              │
                              ▼
                        CommandHandler::execute()
```

## Structure des fichiers

```
inc/
├── Server.hpp      # Classe principale (déjà exists)
├── Client.hpp     # NOUVEAU - Gestion client
├── Channel.hpp    # NOUVEAU - Gestion canaux
├── Message.hpp    # NOUVEAU - Parsing messages
└── CommandHandler.hpp  # NOUVEAU - Traitement commandes

src/
├── Server.cpp     # Boucle principale, sockets
├── Client.cpp     # Implémentation Client
├── Channel.cpp    # Implémentation Channel
├── Message.cpp    # Parsing IRC
├── CommandHandler.cpp  # Commandes IRC
└── main.cpp       # Point d'entrée
```

## Commandes IRC implémentées

| Commande | Description |
|----------|-------------|
| PASS | Authentification mot de passe |
| NICK | Définition du nickname |
| USER | Définition identité utilisateur |
| JOIN | Rejoindre un canal |
| PART | Quitter un canal |
| PRIVMSG | Envoyer message |
| NOTICE | Envoyer notice |
| KICK | Exclure utilisateur |
| INVITE | Inviter utilisateur |
| TOPIC | Topic du canal |
| MODE | Modes du canal |
| QUIT | Déconnexion |
| PING | Keepalive |
| WHO | Liste utilisateurs |

## Test du serveur

```bash
# Compiler
make re

# Démarrer
./ircserv 6667 password &

# Tester avec nc
nc localhost 6667
PASS password
NICK test
USER test 0 * :Test User

# Le serveur répond avec:
:localhost 001 test :Welcome...
```

## Points clés à retenir

1. **poll()** permet de gérer plusieurs clients avec un seul thread
2. **O_NONBLOCK** évite que le serveur se bloque sur I/O
3. **Le buffer** est indispensable car TCP fragmente les données
4. **\r\n** est le délimiteur de messages IRC
5. **forward declaration** suffit pour les pointeurs, mais pas pour l'utilisation complète des objets