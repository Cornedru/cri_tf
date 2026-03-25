# Étude Fonctionnelle — ft_irc

> Projet 42 — Serveur IRC en C++98. Ce document est destiné à l'équipe pour comprendre l'objectif, l'architecture, la roadmap et la répartition des rôles.

---

## Sommaire

- [Vue d'ensemble](#vue-densemble)
- [Architecture du système](#architecture-du-système)
- [Contraintes critiques](#contraintes-critiques)
- [Roadmap par phases](#roadmap-par-phases)
- [Répartition des rôles](#répartition-des-rôles)
- [Interfaces communes à définir](#interfaces-communes-à-définir)
- [Plan de tests](#plan-de-tests)
- [Codes de réponse IRC — référence rapide](#codes-de-réponse-irc--référence-rapide)

---

## Vue d'ensemble

**Objectif** : implémenter un serveur IRC (Internet Relay Chat) complet en C++98, capable de gérer plusieurs clients simultanément via une architecture I/O non-bloquante. Un seul processus, un seul `poll()`, zéro thread, zéro fork.

**Ce que le serveur doit faire :**
- Accepter plusieurs connexions TCP simultanées
- Authentifier les clients (PASS → NICK → USER)
- Gérer des canaux de discussion avec opérateurs
- Relayer les messages entre clients d'un même canal
- Implémenter les modes de canal : `+i` `+t` `+k` `+o` `+l`
- Supporter les commandes opérateur : `KICK`, `INVITE`, `TOPIC`, `MODE`

**Ce que le serveur ne doit PAS faire :**
- Implémenter un client IRC
- Gérer la communication serveur-à-serveur
- Utiliser des threads, des forks, des bibliothèques externes

**Exécution :**
```bash
./ircserv <port> <password>
# Exemple :
./ircserv 6667 monpassword
```

---

## Architecture du système

```
Clients (irssi / HexChat / nc)
        │
        │  TCP/IP non-bloquant (O_NONBLOCK)
        ▼
┌─────────────────────────────────────────────────────┐
│                      Server                         │
│                                                     │
│  poll() ─── unique, surveille tous les fd           │
│     │                                               │
│     ├── accept()      → nouveau Client              │
│     ├── recv()        → buffer d'accumulation       │
│     ├── parse()       → Message (prefix/cmd/params) │
│     ├── dispatch()    → CommandHandler              │
│     └── send()        → réponses \r\n               │
│                                                     │
│  _clients : map<int fd, Client*>                    │
│  _channels : map<string nom, Channel*>              │
│  _pollfds  : vector<pollfd>  ← grandit dynamiquement│
└─────────────────────────────────────────────────────┘
        │
        ├── CommandHandler (stateless, méthodes statiques)
        │       PASS / NICK / USER / JOIN / PART
        │       PRIVMSG / NOTICE / QUIT / PING
        │       KICK / INVITE / TOPIC / MODE / WHO
        │
        ├── Client
        │       fd, nick, user, hostname
        │       _inputBuffer (accumulation données partielles)
        │       _authenticated, _registered
        │
        ├── Channel
        │       nom, topic, key, userLimit
        │       _clients (set), _operators (set), _invited (set)
        │       modes : +i +t +k +l
        │
        └── Message
                prefix, command, params[]
                parse(raw) → static factory
```

### Format des messages IRC

```
[:prefix] COMMAND [param1 param2 ...] [:trailing]\r\n

Exemples :
  PASS monpassword
  NICK alice
  USER alice 0 * :Alice Dupont
  :alice!alice@127.0.0.1 PRIVMSG #general :Hello world
  :alice!alice@127.0.0.1 JOIN #general
  :server 001 alice :Welcome to the IRC Network
```

> **Point critique** : les messages arrivent potentiellement en plusieurs paquets TCP. Le buffer côté `Client` accumule les données jusqu'à trouver `\r\n`. C'est le test obligatoire du sujet (voir [Plan de tests](#plan-de-tests)).

---

## Contraintes critiques

Ces règles entraînent une **note de 0** si elles ne sont pas respectées :

| Contrainte | Détail |
|---|---|
| 🔴 Pas de `fork()` | Tout dans un seul processus |
| 🔴 Un seul `poll()` | Surveille listen + tous les clients dans un seul appel |
| 🔴 I/O non-bloquant | `fcntl(fd, F_SETFL, O_NONBLOCK)` sur chaque fd. Gérer `EAGAIN` |
| 🔴 Pas de crash | Même en out-of-memory. Wrapper les allocations critiques |
| 🔵 C++98 strict | Flag `-std=c++98`. Pas de `auto`, lambda, `nullptr`, range-for |
| 🔵 Pas de lib externe | Ni Boost, ni rien d'autre |
| 🟢 Makefile complet | Règles `$(NAME)`, `all`, `clean`, `fclean`, `re`. Flags : `-Wall -Wextra -Werror` |

---

## Roadmap par phases

### Phase 1 — Fondations réseau (1–2 jours)

> **Bloquer** les phases suivantes — à faire ensemble pour valider la base.

- [x] Créer le socket TCP : `socket()`, `bind()`, `listen()`, `fcntl(O_NONBLOCK)`, `SO_REUSEADDR`
- [x] Boucle `poll()` unique : accepter les connexions entrantes
- [x] Squelette `Client` : fd, buffer, `recv()` non-bloquant
- [x] Test : `nc -C localhost 6667` → connexion brute sans crash

```cpp
// Vérifier que poll() gère bien EINTR
int n = poll(&_pollfds[0], _pollfds.size(), -1);
if (n < 0 && errno == EINTR) continue;  // signal reçu, on continue
```

---

### Phase 2 — Buffer et parsing IRC (1–2 jours)

> **Critique** : gestion des données partielles (test `nc` obligatoire).

- [~] `Client::extractMessages()` — split sur `\r\n`, buffer partiel conservé
- [~] `Message::parse()` — extraction prefix / command / params / trailing
- [~] `Server::dispatch()` — routing vers `CommandHandler`

**Test obligatoire du sujet :**
```bash
nc -C 127.0.0.1 6667
# Taper "COM" puis Ctrl+D  → buffer = "COM"
# Taper "MAN" puis Ctrl+D  → buffer = "COMMAN"
# Taper "D" puis Entrée    → buffer = "COMMAND\r\n" → traité
```

---

### Phase 3 — Enregistrement client (1–2 jours)

> Séquence obligatoire : **PASS → NICK → USER** → messages 001–004.

- [ ] `PASS` — vérification mot de passe → `464` si incorrect
- [ ] `NICK` — unicité du nick → `433` si déjà pris
- [ ] `USER` — username + realname
- [ ] `sendWelcomeMessages()` — codes `001` `002` `003` `004`
- [ ] Test : connexion complète avec irssi ou HexChat

```
Client → Serveur : PASS monpassword
Client → Serveur : NICK alice
Client → Serveur : USER alice 0 * :Alice Dupont
Serveur → Client : :localhost 001 alice :Welcome to the IRC Network alice!alice@127.0.0.1
Serveur → Client : :localhost 002 alice :Your host is localhost
Serveur → Client : :localhost 003 alice :This server was created ...
Serveur → Client : :localhost 004 alice localhost 1.0 + :
```

---

### Phase 4 — Canaux : JOIN / PART / PRIVMSG / QUIT (2–3 jours)

> Broadcast multi-client, création et suppression automatique des canaux.

- [ ] Classe `Channel` — membres, broadcast, gestion des opérateurs
- [ ] `JOIN` — créer le canal si inexistant, `353` NAMREPLY, `366`, `331`/`332`
- [ ] `PART` — quitter le canal, supprimer le canal si vide
- [ ] `PRIVMSG` — message vers utilisateur ou canal (`401`/`404`)
- [ ] `QUIT` — déconnexion propre, notification de tous les canaux
- [ ] Test : 2 clients irssi dans le même canal, échange de messages

> Le premier utilisateur à rejoindre un canal devient automatiquement opérateur.

---

### Phase 5 — Commandes opérateurs (2–3 jours)

> Modes obligatoires selon le sujet : `+i` `+t` `+k` `+o` `+l`.

- [ ] `MODE` — parser les flags `+/-` et leurs paramètres optionnels
- [ ] Mode `+i` — invite-only : impact sur `JOIN` et `INVITE`
- [ ] Mode `+t` — topic restreint aux opérateurs
- [ ] Mode `+k` — clé de canal : vérification au `JOIN`
- [ ] Mode `+l` — limite d'utilisateurs : refus au `JOIN` si atteinte
- [ ] Mode `+o` / `-o` — donner/retirer le statut opérateur
- [ ] `KICK` — vérif opérateur, code `482`, notification canal
- [ ] `INVITE` — liste des invités, `RPL_INVITING` 341
- [ ] `TOPIC` — afficher / modifier, restriction `+t`

**Scénario de test complet :**
```
Op A : MODE #secret +i           → canal invite-only
Op A : INVITE bob #secret        → bob ajouté à la liste
Bob  : JOIN #secret              → OK (invité)
User C : JOIN #secret            → 473 (non invité)
Op A : KICK #secret bob :bye     → bob exclu
```

---

### Phase 6 — Finitions et robustesse (1–2 jours)

- [ ] `PING` → `PONG` avec le même token
- [ ] `NOTICE` — silencieux sur erreur
- [ ] `WHO` — `RPL_WHOREPLY` 352 + `315`
- [ ] Tests de robustesse : déconnexion brutale (Ctrl+C), spam JOIN/PART, données partielles
- [ ] `README.md` — description, instructions, ressources, usage de l'IA

---

## Répartition des rôles

> Suggestion pour 3 développeurs. Les phases 1–2 bénéficient d'un travail en binôme pour valider les fondations.

### Dev A — Infra réseau & architecture

| Responsabilité | Fichiers principaux |
|---|---|
| Socket, `poll()`, `accept()` — boucle principale | `Server.cpp` / `Server.hpp` |
| `Server::dispatch()`, `removeClient()` | `Server.cpp` |
| Classe `Channel` — broadcast, opérateurs, modes `+i +t +k +l` | `Channel.cpp` / `Channel.hpp` |
| Parsing `MODE` et application des flags | `CommandHandler.cpp` |
| `Makefile` + structure des fichiers | `Makefile` |

### Dev B — I/O & messagerie

| Responsabilité | Fichiers principaux |
|---|---|
| Classe `Client` — buffer, `extractMessages()`, `sendMsg()` | `Client.cpp` / `Client.hpp` |
| `Message::parse()` — parsing IRC complet | `Message.cpp` / `Message.hpp` |
| `PRIVMSG` / `NOTICE` — routing user et canal | `CommandHandler.cpp` |
| `QUIT` — déconnexion propre, `KICK`, `INVITE` | `CommandHandler.cpp` |
| `PING` → `PONG`, gestion `EINTR`/`EAGAIN` | `CommandHandler.cpp` |

### Dev C — Commandes & enregistrement

| Responsabilité | Fichiers principaux |
|---|---|
| `PASS` / `NICK` / `USER` + welcome 001–004 | `CommandHandler.cpp` |
| `JOIN` / `PART` — `353`, `366`, `331`/`332`, canal vide | `CommandHandler.cpp` |
| `TOPIC` — affichage et restriction `+t` | `CommandHandler.cpp` |
| `MODE +o / -o`, `WHO` (352 + 315) | `CommandHandler.cpp` |
| Tous les codes d'erreur 4xx + `README.md` | `CommandHandler.cpp` |

---

## Interfaces communes à définir

> **À faire le jour 1, ensemble.** Un header partagé évite 80% des conflits de merge.

```cpp
// Server.hpp
Client*  getClientByNick(const std::string& nick) const;
Channel* getChannel(const std::string& name) const;
void     addChannel(Channel* channel);
void     removeChannel(const std::string& name);
void     removeClient(int fd);

// Client.hpp
void        sendMsg(const std::string& msg);
void        appendToBuffer(const std::string& data);
std::vector<std::string> extractMessages();
std::string getPrefix() const;  // retourne "nick!user@host"

// Channel.hpp
void broadcast(const std::string& msg, Client* except = NULL);
bool hasClient(Client* c) const;
bool isOperator(Client* c) const;
bool isInvited(Client* c) const;
void addClient(Client* c);
void removeClient(Client* c);

// Message.hpp
static Message parse(const std::string& raw);
const std::string& getCommand() const;
const std::vector<std::string>& getParams() const;
```

---

## Plan de tests

### Connexion et authentification

```bash
# Connexion brute sans PASS → commandes ignorées
nc localhost 6667
NICK test         # doit être ignoré (non authentifié)

# PASS incorrect
nc -C localhost 6667
PASS mauvais      # → :localhost 464 * :Password incorrect

# Nick déjà pris
# Client A : PASS p / NICK alice / USER a 0 * :A
# Client B : PASS p / NICK alice
# Client B reçoit : :localhost 433 * alice :Nickname is already in use

# Double enregistrement
# Après USER : renvoyer USER → :localhost 462 alice :You may not reregister
```

### Données partielles (test obligatoire)

```bash
nc -C 127.0.0.1 6667
# "COM" Ctrl+D → pas de traitement
# "MAN" Ctrl+D → pas de traitement
# "D"   Entrée → "COMMAND\r\n" traité
```

### Canaux

```bash
# JOIN normal
JOIN #test
# → :alice!alice@127.0.0.1 JOIN #test
# → :localhost 331 alice #test :No topic is set
# → :localhost 353 alice = #test :@alice
# → :localhost 366 alice #test :End of /NAMES list

# Canal +i sans invitation
MODE #secret +i
JOIN #secret    # (autre client) → 473

# Canal +k mauvaise clé
MODE #test +k secretkey
JOIN #test badkey   # → 475

# Canal +l plein
MODE #test +l 2
# (3ème client) JOIN #test → 471

# PART → canal vide → canal supprimé
# Vérifier qu'un nouveau JOIN recrée le canal proprement
```

### Commandes opérateurs

```bash
# KICK par non-opérateur → 482
# KICK par opérateur → OK + notification canal

# INVITE
INVITE bob #general
# → :localhost 341 alice bob #general
# → bob reçoit : :alice!alice@127.0.0.1 INVITE bob #general

# TOPIC avec +t par non-op → 482
# TOPIC avec +t par op → OK + broadcast

# MODE +o → alice devient opérateur
MODE #test +o alice
# → alice peut maintenant KICK, INVITE, MODE
```

### Robustesse

```bash
# Déconnexion brutale (Ctrl+C côté client)
# → recv() retourne 0 → removeClient() → canal notifié

# Spam JOIN/PART rapide → pas de crash, pas de fuite mémoire

# Connexion sans envoyer rien → pas de blocage du serveur
```

---

## Codes de réponse IRC — référence rapide

### Succès

| Code | Nom | Contexte |
|---|---|---|
| 001 | RPL_WELCOME | Enregistrement complet |
| 002 | RPL_YOURHOST | Enregistrement complet |
| 003 | RPL_CREATED | Enregistrement complet |
| 004 | RPL_MYINFO | Enregistrement complet |
| 315 | RPL_ENDOFWHO | Fin de WHO |
| 324 | RPL_CHANNELMODEIS | Réponse MODE |
| 331 | RPL_NOTOPIC | JOIN / TOPIC sans topic |
| 332 | RPL_TOPIC | JOIN / TOPIC avec topic |
| 341 | RPL_INVITING | INVITE réussi |
| 352 | RPL_WHOREPLY | WHO |
| 353 | RPL_NAMREPLY | JOIN — liste des membres |
| 366 | RPL_ENDOFNAMES | JOIN — fin de liste |

### Erreurs

| Code | Nom | Contexte |
|---|---|---|
| 401 | ERR_NOSUCHNICK | PRIVMSG / INVITE — cible inexistante |
| 403 | ERR_NOSUCHCHANNEL | Canal inexistant |
| 404 | ERR_CANNOTSENDTOCHAN | PRIVMSG — pas dans le canal |
| 431 | ERR_NONICKNAMEGIVEN | NICK sans paramètre |
| 432 | ERR_ERRONEUSNICKNAME | NICK — caractères invalides |
| 433 | ERR_NICKNAMEINUSE | NICK — déjà utilisé |
| 441 | ERR_USERNOTINCHANNEL | KICK — cible pas dans le canal |
| 442 | ERR_NOTONCHANNEL | PART / TOPIC / INVITE — pas membre |
| 443 | ERR_USERONCHANNEL | INVITE — déjà dans le canal |
| 461 | ERR_NEEDMOREPARAMS | Paramètres manquants |
| 462 | ERR_ALREADYREGISTERED | PASS / USER après enregistrement |
| 464 | ERR_PASSWDMISMATCH | PASS incorrect |
| 471 | ERR_CHANNELISFULL | JOIN — limite `+l` atteinte |
| 473 | ERR_INVITEONLYCHAN | JOIN — canal `+i` sans invitation |
| 475 | ERR_BADCHANNELKEY | JOIN — mauvaise clé `+k` |
| 482 | ERR_CHANOPRIVSNEEDED | KICK / TOPIC / MODE — pas opérateur |

---

## Ressources

- [RFC 1459](https://datatracker.ietf.org/doc/html/rfc1459) — protocole IRC original
- [RFC 2812](https://datatracker.ietf.org/doc/html/rfc2812) — protocole IRC révisé
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) — référence sockets en C
- `man 2 poll` / `man 2 recv` / `man 2 fcntl` — pages de manuel POSIX

---

*Document rédigé pour l'équipe projet ft_irc — 42 School.*
