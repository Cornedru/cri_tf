=== FICHIER : DOCUMENTATION_TECHNIQUE.md ===

# Documentation Technique — ft_irc

## 1. Vue d'ensemble du projet

### 1.1 Objectif fonctionnel

ft_irc est un serveur IRC (Internet Relay Chat) complet implémentant le protocole défini dans les RFC 1459 et 2812. Il permet à plusieurs clients de se connecter, de rejoindre des canaux de discussion, et d'échanger des messages en temps réel. Le serveur gère l'authentification par mot de passe, la gestion des canaux (création, modes, opérateurs), et le relayage des messages entre clients.

### 1.2 Architecture du système

[📖 Voir la pipeline →](./pipeline/ft_irc_interactive_pipeline.html)

### 1.3 Contraintes techniques

| Contrainte | Valeur | Raison |
|------------|--------|--------|
| Langage | C++98 (-std=c++98) | Conformité au sujet 42 |
| Compilateur | GCC avec -Wall -Wextra -Werror | Code sans avertissements |
| Threads | 0 | Interdit par le sujet |
| Fork | 0 | Interdit par le sujet |
| I/O | O_NONBLOCK | Non-bloquant, un seul thread |
| Boucle réseau | poll() unique | Gestion centralisée de tous les fd |
| Bibliothèques externes | Aucune | Autonomie complète |

---

## 2. Pipeline de traitement — étape par étape

### 2.1 Initialisation du serveur

La phase d'initialisation crée le socket d'écoute, le lie à une adresse, et le configure en mode non-bloquant. Cette séquence est critique : si une étape échoue, le serveur doit nettoyer les ressources déjà allouées et signaler l'erreur.

```cpp
// Server.cpp - Constructeur
int Server::Server(int port, const std::string& password)
    : _port(port), _password(password), _sockfd(-1) {

    // Création du socket TCP
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) {
        throw std::runtime_error("socket() failed");
    }

    // Activation de SO_REUSEADDR pour permettre le restart rapide
    int opt = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configuration de l'adresse (INADDR_ANY = toutes les interfaces)
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);

    // Binding du socket
    if (bind(_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(_sockfd);
        throw std::runtime_error("bind() failed");
    }

    // Mise en écoute (backlog = 10)
    if (listen(_sockfd, 10) < 0) {
        close(_sockfd);
        throw std::runtime_error("listen() failed");
    }

    // Configuration non-bloquante O_NONBLOCK
    int flags = fcntl(_sockfd, F_GETFL, 0);
    fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);

    // Ajout du socket d'écoute au vecteur pollfd
    struct pollfd pfd;
    pfd.fd = _sockfd;
    pfd.events = POLLIN;  // Surveiller les connexions entrantes
    pfd.revents = 0;
    _pollfds.push_back(pfd);
}
```

**Cas d'erreur à gérer :**
- Échec de socket() → lever une exception
- Échec de bind() → fermer le socket et lever une exception
- Port déjà utilisé → SO_REUSEADDR permet généralement de contourner
- Échec de fcntl() → journaliser l'erreur mais continuer

### 2.2 Boucle poll()

Le serveur utilise un seul appel à poll() pour surveiller tous les descripteurs de fichiers simultanément. Cette approche est plus efficace que select() car elle n'a pas de limite sur le nombre de fd et gère finement les événements. La boucle est infinie et ne s'arrête que sur une erreur système (autre que EINTR).

```cpp
// Server.cpp - Méthode run()
void Server::run() {
    while (true) {
        // poll() bloque jusqu'à ce qu'un événement se produise
        // Timeout de -1 = attente infinie
        int n = poll(&_pollfds[0], _pollfds.size(), -1);

        if (n < 0) {
            // EINTR = interruption par un signal, on continue
            if (errno == EINTR) {
                continue;
            }
            // Toute autre erreur termine la boucle
            break;
        }

        // Itération sur tous les fd surveillés
        for (size_t i = 0; i < _pollfds.size(); ++i) {
            // Vérification de POLLIN (données disponibles en lecture)
            if (!(_pollfds[i].revents & POLLIN)) {
                continue;
            }

            // Distinction : socket d'écoute vs client connecté
            if (_pollfds[i].fd == _sockfd) {
                acceptClient();      // Nouvelle connexion
            } else {
                handleClientData(_pollfds[i].fd);  // Données client
            }
        }
    }
}
```

**Points clés :**
- `_pollfds` est un vecteur dynamique qui grandit avec les connexions
- Chaque client ajouté insert un nouveau pollfd
- Chaque client supprimé retire son pollfd du vecteur
- L'ordre dans le vecteur n'est pas important (on itère sur tous)

### 2.3 Acceptation de nouvelles connexions

Lorsqu'un événement POLLIN est détecté sur le socket d'écoute, accept() est appelé pour récupérer la nouvelle connexion. Le client est créé, ajouté à la map des clients, et son fd est ajouté à poll().

```cpp
void Server::acceptClient() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    // accept() est non-bloquant, retourne -1 si pas de connexion en attente
    int clientFd = accept(_sockfd, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientFd < 0) {
        return;
    }

    // Configuration non-bloquante du nouveau client
    int flags = fcntl(clientFd, F_GETFL, 0);
    fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);

    // Extraction de l'adresse IP du client
    char hostname[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, hostname, INET_ADDRSTRLEN);

    // Création de l'objet Client et ajout à la map
    Client* client = new Client(clientFd, std::string(hostname));
    _clients[clientFd] = client;

    // Ajout du fd client à poll()
    struct pollfd pfd;
    pfd.fd = clientFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollfds.push_back(pfd);
}
```

**Cas d'erreur à gérer :**
- EAGAIN/EWOULDBLOCK : pas de connexion en attente (normal en non-bloquant)
- EMFILE : trop de fichiers ouverts
- ENOMEM : mémoire insuffisante

### 2.4 Réception des données (recv())

La lecture des données clients utilise recv() en mode non-bloquant. Le comportement diffère selon la valeur de retour : 0 = déconnexion, -1 avec errno=EAGAIN = pas de données (retry), -1 avec autre errno = erreur.

```cpp
void Server::handleClientData(int fd) {
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));

    // recv() non-bloquant
    int ret = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (ret <= 0) {
        // ret == 0 : client a fermé la connexion
        // ret < 0 && errno != EAGAIN : erreur réseau
        if (ret < 0 && errno == EAGAIN) {
            return;  // Pas de données, ce n'est pas une erreur
        }
        removeClient(fd);  // Déconnexion propre
        return;
    }

    // Ajout des données reçues au buffer du client
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) {
        removeClient(fd);
        return;
    }

    Client* client = it->second;
    client->appendToBuffer(std::string(buffer, ret));

    // Extraction des messages complets (\r\n)
    std::vector<std::string> messages = client->extractMessages();
    for (size_t i = 0; i < messages.size(); ++i) {
        Message msg = Message::parse(messages[i]);
        if (!msg.getCommand().empty()) {
            dispatch(msg, *client);
        }
    }
}
```

### 2.5 Buffering partiel et split sur \r\n

Le protocole IRC utilise \r\n comme délimiteur de messages. Les données arrivent potentiellement en plusieurs morceaux (paquets TCP fragmentés). Le buffer d'entrée du client accumule les données jusqu'à obtenir un \r\n complet.

```cpp
// Client.cpp - extractMessages()
std::vector<std::string> Client::extractMessages() {
    std::vector<std::string> messages;
    std::string::size_type pos;

    // Recherche de \r\n dans le buffer
    while ((pos = _inputBuffer.find("\r\n")) != std::string::npos) {
        // Extraction du message complet
        std::string msg = _inputBuffer.substr(0, pos);
        // Suppression du message + \r\n du buffer
        _inputBuffer.erase(0, pos + 2);
        if (!msg.empty()) {
            messages.push_back(msg);
        }
    }
    // Les données partielles restent dans _inputBuffer
    return messages;
}
```

**Exemple du test nc -C obligatoire :**

```bash
$ nc -C 127.0.0.1 6667
COM    # Ctrl+D = EOF = transmission de "COM"
MAN    # Ctrl+D = transmission de "MAN"
D      # Entrée = transmission de "D\r\n"

# Le serveur reçoit :
# 1. "COM"     → pas de \r\n, reste en buffer
# 2. "MAN"     → pas de \r\n, buffer = "COMMAN"
# 3. "D\r\n"   → trouve \r\n, traite "COMMAND\r\n"
```

### 2.6 Parsing des messages IRC

Le format IRC est : `[:prefix] COMMAND [param1 param2 ... [:trailing]]`

```cpp
// Message.cpp - parse()
Message Message::parse(const std::string& raw) {
    Message msg;
    std::string str = raw;

    if (str.empty()) {
        return msg;
    }

    // Extraction du prefix (optionnel, commence par ':')
    if (str[0] == ':') {
        std::string::size_type space = str.find(' ');
        if (space != std::string::npos) {
            msg._prefix = str.substr(1, space - 1);
            str = str.substr(space + 1);
        }
    }

    // Extraction de la commande
    std::string::size_type space = str.find(' ');
    if (space == std::string::npos) {
        msg._command = str;
        return msg;
    }

    msg._command = str.substr(0, space);
    str = str.substr(space + 1);

    // Extraction des paramètres
    while (!str.empty()) {
        // Le paramètre commençant par ':' est le "trailing" (contient des espaces)
        if (str[0] == ':') {
            msg._params.push_back(str.substr(1));
            break;
        }

        space = str.find(' ');
        if (space == std::string::npos) {
            msg._params.push_back(str);
            break;
        }

        msg._params.push_back(str.substr(0, space));
        str = str.substr(space + 1);
    }

    return msg;
}
```

**Exemples de parsing :**

| Message brut | Prefix | Command | Params |
|--------------|--------|---------|--------|
| `PASS mypass` | (vide) | PASS | ["mypass"] |
| `:nick!user@host PRIVMSG #chan :hello world` | nick!user@host | PRIVMSG | ["#chan", "hello world"] |
| `JOIN #general` | (vide) | JOIN | ["#general"] |
| `:server 001 nick :Welcome` | server | 001 | ["nick", "Welcome"] |

### 2.7 Séquence d'enregistrement du client

Un client doit s'enregistrer dans l'ordre : PASS → NICK → USER. Le serveur refuse les commandes si l'ordre n'est pas respecté ou si le mot de passe est incorrect.

```cpp
// CommandHandler.cpp - PASS
void CommandHandler::PASS(const Message& msg, Client& client, Server& server) {
    const std::vector<std::string>& params = msg.getParams();

    if (params.empty()) {
        client.sendMsg(std::string(":") + server.getHostname() + " 461 " +
                       client.getNick() + " PASS :Not enough parameters");
        return;
    }

    if (client.isAuthenticated()) {
        client.sendMsg(std::string(":") + server.getHostname() + " 462 " +
                       client.getNick() + " :You may not reregister");
        return;
    }

    if (params[0] != server.getPassword()) {
        client.sendMsg(std::string(":") + server.getHostname() + " 464 " +
                       client.getNick() + " :Password incorrect");
        return;
    }

    client.setAuthenticated(true);
}
```

```cpp
// CommandHandler.cpp - NICK
void CommandHandler::NICK(const Message& msg, Client& client, Server& server) {
    // ... validation du nickname ...

    if (server.getClientByNick(newNick)) {
        client.sendMsg(std::string(":") + server.getHostname() + " 433 " +
                       client.getNick() + " " + newNick + " :Nickname is already in use");
        return;
    }

    client.setNick(newNick);

    // Envoi des messages de bienvenue si enregistrement complet
    if (client.isAuthenticated() && !client.getUser().empty()) {
        client.setRegistered(true);
        sendWelcomeMessages(client);
    }
}
```

**Messages de bienvenue (001-004) :**

```cpp
void CommandHandler::sendWelcomeMessages(Client& client) {
    std::string nick = client.getNick();
    std::string prefix = ":" + std::string("localhost");

    client.sendMsg(prefix + " 001 " + nick + " :Welcome to the IRC Network " +
                   nick + "!" + client.getUser() + "@" + client.getHostname());
    client.sendMsg(prefix + " 002 " + nick + " :Your host is localhost");
    client.sendMsg(prefix + " 003 " + nick + " :This server was created " + getTime());
    client.sendMsg(prefix + " 004 " + nick + " localhost 1.0 + :");
}
```

### 2.8 Dispatch des commandes

Le CommandHandler reçoit le message parsé et appelle la fonction appropriée selon la commande.

```cpp
void CommandHandler::execute(const Message& msg, Client& client, Server& server) {
    std::string cmd = msg.getCommand();

    if (cmd == "PASS") {
        PASS(msg, client, server);
    } else if (cmd == "NICK") {
        NICK(msg, client, server);
    } else if (cmd == "USER") {
        USER(msg, client, server);
    } else if (cmd == "JOIN") {
        JOIN(msg, client, server);
    }
    // ... autres commandes ...
}
```

### 2.9 Envoi des réponses (send())

Toutes les réponses utilisent send() non-bloquant. Le format标准 est `:<serveur> <code> <nick> <message>\r\n` pour les réponses numériques.

```cpp
// Client.cpp - sendMsg()
void Client::sendMsg(const std::string& msg) {
    std::string toSend = msg;
    // Ajout de \r\n si absent
    if (toSend.substr(toSend.size() - 2) != "\r\n") {
        toSend += "\r\n";
    }
    send(_fd, toSend.c_str(), toSend.size(), 0);
}
```

**Exemples de messages sortants :**

```
:localhost 001 testuser :Welcome to the IRC Network testuser!testuser@127.0.0.1
:testuser!user@127.0.0.1 PRIVMSG #general :Hello everyone!
:testuser!user@127.0.0.1 JOIN #general
:alice!alice@127.0.0.1 KICK #general bob :Get out!
```

### 2.10 Déconnexion propre

La déconnexion se fait soit par commande QUIT explicite, soit par fermeture de connexion TCP. Dans les deux cas, le client doit être retiré de tous les canaux et la mémoire libérée.

```cpp
// Server.cpp - removeClient()
void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) {
        return;
    }

    Client* client = it->second;

    // Retirer le client de tous les canaux
    std::map<std::string, Channel*>::iterator cit;
    std::vector<std::string> channelsToRemove;

    for (cit = _channels.begin(); cit != _channels.end(); ++cit) {
        Channel* chan = cit->second;
        if (chan->hasClient(client)) {
            std::string quitMsg = ":" + client->getPrefix() + " QUIT :Client closed connection";
            chan->broadcast(quitMsg, client);
            chan->removeClient(client);
            if (chan->getClients().empty()) {
                channelsToRemove.push_back(cit->first);
            }
        }
    }

    // Supprimer les canaux vides
    for (size_t i = 0; i < channelsToRemove.size(); ++i) {
        delete _channels[channelsToRemove[i]];
        _channels.erase(channelsToRemove[i]);
    }

    // Fermer le fd et nettoyer
    close(fd);

    // Retirer de poll()
    for (size_t i = 0; i < _pollfds.size(); ++i) {
        if (_pollfds[i].fd == fd) {
            _pollfds.erase(_pollfds.begin() + i);
            break;
        }
    }

    // Libérer la mémoire
    delete client;
    _clients.erase(fd);
}
```

---

## 3. Documentation des classes

### 3.1 Server

**Rôle :** Gère le socket d'écoute, la boucle principale poll(), l'acceptation des connexions, et le dispatch des messages vers les handlers.

**Attributs privés :**

| Attribut | Type | Description |
|----------|------|-------------|
| `_port` | int | Port d'écoute du serveur |
| `_password` | std::string | Mot de passe requis pour la connexion |
| `_sockfd` | int | Descripteur du socket d'écoute |
| `_clients` | std::map<int, Client*> | Map fd → client connecté |
| `_channels` | std::map<std::string, Channel*> | Map nom → canal |
| `_pollfds` | std::vector<struct pollfd> | Vecteur des fd surveillés par poll() |

**Méthodes publiques :**

| Méthode | Signature | Description |
|---------|-----------|-------------|
| Constructeur | `Server(int port, const std::string& password)` | Initialise le socket, le bind, et listen |
| run | `void run()` | Boucle principale avec poll() |
| acceptClient | `void acceptClient()` | Accepte une nouvelle connexion |
| removeClient | `void removeClient(int fd)` | Ferme un client et nettoie les canaux |
| dispatch | `void dispatch(const Message& msg, Client& client)` | Envoie le message au CommandHandler |
| handleClientData | `void handleClientData(int fd)` | Lit et traite les données d'un client |
| getClient | `Client* getClient(int fd) const` | Retourne le client par fd |
| getClientByNick | `Client* getClientByNick(const std::string& nick) const` | Retourne le client par nickname |
| getChannel | `Channel* getChannel(const std::string& name) const` | Retourne le canal par nom |
| addChannel | `void addChannel(Channel* channel)` | Ajoute un canal |
| removeChannel | `void removeChannel(const std::string& name)` | Supprime un canal |
| getHostname | `const std::string& getHostname()` | Retourne le nom d'hôte du serveur |

**Interactions :**
- Crée et détruit les objets Client lors des connexions/déconnexions
- Crée les objets Channel lors du premier JOIN
- Utilise CommandHandler::execute() pour traiter les messages

---

### 3.2 Client

**Rôle :** Représente un client connecté, gère son buffer d'entrée, son état d'enregistrement, et l'envoi des réponses.

**Attributs privés :**

| Attribut | Type | Description |
|----------|------|-------------|
| `_fd` | int | Descripteur de fichier de la connexion |
| `_nick` | std::string | Nickname du client |
| `_user` | std::string | Nom d'utilisateur |
| `_realname` | std::string | Nom réel (realname) |
| `_hostname` | std::string | Adresse IP du client |
| `_inputBuffer` | std::string | Buffer d'accumulation des données partielles |
| `_authenticated` | bool | true après réception du bon PASS |
| `_registered` | bool | true après NICK + USER valides |

**Méthodes publiques :**

| Méthode | Signature | Description |
|---------|-----------|-------------|
| Constructeur | `Client(int fd, const std::string& hostname)` | Initialise le client avec son fd et IP |
| sendMsg | `void sendMsg(const std::string& msg)` | Envoie un message au client |
| extractMessages | `std::vector<std::string> extractMessages()` | Extrait les messages complets du buffer |
| isRegistered | `bool isRegistered() const` | Retourne l'état d'enregistrement |
| isAuthenticated | `bool isAuthenticated() const` | Retourne l'état d'authentification |
| getFd | `int getFd() const` | Retourne le fd |
| getNick | `const std::string& getNick() const` | Retourne le nickname |
| getUser | `const std::string& getUser() const` | Retourne le username |
| getRealname | `const std::string& getRealname() const` | Retourne le realname |
| getHostname | `const std::string& getHostname() const` | Retourne l'IP |
| getPrefix | `std::string getPrefix() const` | Retourne le prefix IRC (nick!user@host) |
| setNick | `void setNick(const std::string& nick)` | Définit le nickname |
| setUser | `void setUser(const std::string& user)` | Définit le username |
| setRealname | `void setRealname(const std::string& realname)` | Définit le realname |
| setAuthenticated | `void setAuthenticated(bool value)` | Définit l'état d'authentification |
| setRegistered | `void setRegistered(bool value)` | Définit l'état d'enregistrement |
| appendToBuffer | `void appendToBuffer(const std::string& data)` | Ajoute des données au buffer |

**Interactions :**
- Utilisé par Server pour gérer les connexions
-channel pour le broadcast des messages
- CommandHandler pour envoyer les réponses

---

### 3.3 Channel

**Rôle :** Représente un canal IRC, gère les membres, les opérateurs, les modes, et le topic.

**Attributs privés :**

| Attribut | Type | Description |
|----------|------|-------------|
| `_name` | std::string | Nom du canal (commence par #) |
| `_topic` | std::string | Topic du canal |
| `_key` | std::string | Clé/mot de passe du canal (mode +k) |
| `_userLimit` | int | Limite d'utilisateurs (mode +l, 0 = illimité) |
| `_inviteOnly` | bool | Mode invite-only (+i) |
| `_topicRestricted` | bool | Topic restreint aux ops (+t) |
| `_clients` | std::set<Client*> | Ensemble des membres |
| `_operators` | std::set<Client*> | Ensemble des opérateurs |
| `_invited` | std::set<Client*> | Liste des invités (mode +i) |

**Méthodes publiques :**

| Méthode | Signature | Description |
|---------|-----------|-------------|
| Constructeur | `Channel(const std::string& name)` | Crée un canal avec le nom donné |
| addClient | `void addClient(Client* c)` | Ajoute un client au canal |
| removeClient | `void removeClient(Client* c)` | Retire un client du canal |
| broadcast | `void broadcast(const std::string& msg, Client* except = NULL)` | Envoie à tous les membres |
| isOperator | `bool isOperator(Client* c) const` | Vérifie si client est opérateur |
| hasClient | `bool hasClient(Client* c) const` | Vérifie si client est membre |
| isInvited | `bool isInvited(Client* c) const` | Vérifie si client est invité |
| setMode | `void setMode(char mode, bool add, const std::string& param)` | Active/désactive un mode |
| getName | `const std::string& getName() const` | Retourne le nom |
| getTopic | `const std::string& getTopic() const` | Retourne le topic |
| getKey | `const std::string& getKey() const` | Retourne la clé |
| getUserLimit | `int getUserLimit() const` | Retourne la limite |
| getInviteOnly | `bool getInviteOnly() const` | Retourne état invite-only |
| getTopicRestricted | `bool getTopicRestricted() const` | Retourne état topic restreint |
| getClients | `const std::set<Client*>& getClients() const` | Retourne les membres |
| getOperators | `const std::set<Client*>& getOperators() const` | Retourne les opérateurs |
| setTopic | `void setTopic(const std::string& topic)` | Définit le topic |
| setKey | `void setKey(const std::string& key)` | Définit la clé |
| clearKey | `void clearKey()` | Supprime la clé |
| setUserLimit | `void setUserLimit(int limit)` | Définit la limite |
| clearUserLimit | `void clearUserLimit()` | Supprime la limite |
| addInvite | `void addInvite(Client* c)` | Ajoute à la liste d'invitation |
| removeInvite | `void removeInvite(Client* c)` | Retire de la liste d'invitation |
| addOperator | `void addOperator(Client* c)` | Ajoute comme opérateur |
| removeOperator | `void removeOperator(Client* c)` | Retire le statut opérateur |

**Interactions :**
- Utilisé par Server pour gérer les canaux
- CommandHandler modifie les canaux via JOIN, PART, KICK, INVITE, TOPIC, MODE

---

### 3.4 Message

**Rôle :** Représente un message IRC parsé, avec préfixe, commande et paramètres.

**Attributs privés :**

| Attribut | Type | Description |
|----------|------|-------------|
| `_prefix` | std::string | Préfixe optionnel (nick!user@host) |
| `_command` | std::string | Commande IRC (PASS, JOIN, etc.) |
| `_params` | std::vector<std::string> | Paramètres de la commande |

**Méthodes publiques :**

| Méthode | Signature | Description |
|---------|-----------|-------------|
| Constructeur | `Message()` | Crée un message vide |
| Constructeur | `Message(const std::string& command)` | Crée un message avec commande |
| parse | `static Message parse(const std::string& raw)` | Parse une chaîne IRC |
| getCommand | `std::string getCommand() const` | Retourne la commande |
| getPrefix | `const std::string& getPrefix() const` | Retourne le préfixe |
| getParams | `const std::vector<std::string>& getParams() const` | Retourne les paramètres |
| toString | `std::string toString() const` | Convertit en chaîne IRC |
| setPrefix | `void setPrefix(const std::string& prefix)` | Définit le préfixe |
| setCommand | `void setCommand(const std::string& command)` | Définit la commande |
| addParam | `void addParam(const std::string& param)` | Ajoute un paramètre |

**Interactions :**
- Parser par Server::handleClientData() pour traiter les commandes
- Utilisé par CommandHandler pour accéder à la commande et aux paramètres

---

### 3.5 CommandHandler

**Rôle :** Stateless handler qui traite toutes les commandes IRC. Toutes les méthodes sont statiques.

**Méthodes statiques publiques :**

| Méthode | Description |
|---------|-------------|
| execute | Dispatche vers le handler approprié |
| PASS | Authentification par mot de passe |
| NICK | Définition du nickname |
| USER | Définition de l'identité utilisateur |
| JOIN | Rejoindre un canal |
| PART | Quitter un canal |
| PRIVMSG | Envoyer un message |
| NOTICE | Envoyer une notice (sans réponse automatique) |
| KICK | Exclure un utilisateur d'un canal |
| INVITE | Inviter un utilisateur dans un canal |
| TOPIC | Afficher/modifier le topic |
| MODE | Modifier les modes d'un canal |
| QUIT | Déconnexion du client |
| PING | Requête keepalive |
| WHO | Liste des utilisateurs |

**Interactions :**
- Reçoit les messages parsés par Message::parse()
- Accède à Server pour récupérer/créer des canaux et clients
- Envoie les réponses via client.sendMsg()

---

## 4. Commandes IRC — référence complète

### 4.1 PASS

**Syntaxe :** `PASS <password>`

**Préconditions :** Le client ne doit pas être déjà authentifié.

**Comportement :** Vérifie que le mot de passe correspond à celui du serveur. Si correct, marque le client comme authentifié. Si incorrect, envoie ERR_PASSWDMISMATCH (464).

| Code | Nom | Condition |
|------|-----|-----------|
| 461 | ERR_NEEDMOREPARAMS | Pas de paramètre fourni |
| 462 | ERR_ALREADYREGISTERED | Client déjà authentifié |
| 464 | ERR_PASSWDMISMATCH | Mot de passe incorrect |

**Exemple :**
```
Client → Serveur : PASS mypassword
Serveur → Client : (rien si correct, 464 si incorrect)
```

---

### 4.2 NICK

**Syntaxe :** `NICK <nickname>`

**Préconditions :** Aucune (peut être envoyé avant USER).

**Comportement :** Définit le nickname du client. Si le nickname est déjà utilisé par un autre client, envoie ERR_NICKNAMEINUSE (433). Si le client est déjà enregistré (NICK + USER reçus), envoie les messages de bienvenue.

| Code | Nom | Condition |
|------|-----|-----------|
| 431 | ERR_NONICKNAMEGIVEN | Pas de paramètre |
| 432 | ERR_ERRONEUSNICKNAME | Caractères invalides |
| 433 | ERR_NICKNAMEINUSE | Nickname déjà utilisé |

**Exemple :**
```
Client → Serveur : NICK alice
Serveur → Client : (rien si disponible)
Client → Serveur : NICK bob (si alice existe)
Serveur → Client : :localhost 433 alice bob :Nickname is already in use
```

---

### 4.3 USER

**Syntaxe :** `USER <username> 0 * :<realname>`

**Préconditions :** Le client ne doit pas être déjà enregistré.

**Comportement :** Définit le username et le realname. Si le client est déjà authentifié (PASS validé) et a déjà un NICK, envoie les messages de bienvenue.

| Code | Nom | Condition |
|------|-----|-----------|
| 461 | ERR_NEEDMOREPARAMS | Moins de 4 paramètres |
| 462 | ERR_ALREADYREGISTERED | Client déjà enregistré |

**Exemple :**
```
Client → Serveur : USER bob 0 * :Bob Smith
Client → Serveur : (si PASS et NICK valides)
Serveur → Client : :localhost 001 bob :Welcome to the IRC Network bob!bob@127.0.0.1
```

---

### 4.4 JOIN

**Syntaxe :** `JOIN #<channel> [key]`

**Préconditions :** Client enregistré.

**Comportement :** Le client rejoint le canal. Si le canal n'existe pas, il est créé et le client devient opérateur. Vérifie les modes +i, +k, +l.

| Code | Nom | Condition |
|------|-----|-----------|
| 461 | ERR_NEEDMOREPARAMS | Pas de paramètre |
| 403 | ERR_NOSUCHCHANNEL | Nom de canal invalide |
| 471 | ERR_CHANNELISFULL | Limite atteinte (+l) |
| 473 | ERR_INVITEONLYCHAN | Canal invite-only (+i) |
| 475 | ERR_BADCHANNELKEY | Mauvaise clé (+k) |
| 332 | RPL_TOPIC | Topic du canal |
| 331 | RPL_NOTOPIC | Pas de topic |
| 353 | RPL_NAMREPLY | Liste des utilisateurs |
| 366 | RPL_ENDOFNAMES | Fin de la liste |

**Exemple :**
```
Client → Serveur : JOIN #general
Serveur → Client : :alice!alice@127.0.0.1 JOIN #general
Serveur → Client : :localhost 331 alice #general :No topic is set
Serveur → Client : :localhost 353 alice = #general :@alice
Serveur → Client : :localhost 366 alice #general :End of /NAMES list
```

---

### 4.5 PART

**Syntaxe :** `PART #<channel> [:message]`

**Préconditions :** Client enregistré, membre du canal.

**Comportement :** Le client quitte le canal. Si le canal devient vide, il est supprimé.

| Code | Nom | Condition |
|------|-----|-----------|
| 461 | ERR_NEEDMOREPARAMS | Pas de paramètre |
| 403 | ERR_NOSUCHCHANNEL | Canal inexistant |
| 442 | ERR_NOTONCHANNEL | Client pas dans le canal |

**Exemple :**
```
Client → Serveur : PART #general :Goodbye!
Serveur → Client : :alice!alice@127.0.0.1 PART #general :Goodbye!
```

---

### 4.6 PRIVMSG

**Syntaxe :** `PRIVMSG <target> :<message>`

**Préconditions :** Client enregistré.

**Comportement :** Envoie un message à un utilisateur ou à un canal. Si la cible est un canal, le client doit y être membre.

| Code | Nom | Condition |
|------|-----|-----------|
| 401 | ERR_NOSUCHNICK | Cible inexistante |
| 403 | ERR_NOSUCHCHANNEL | Canal inexistant |
| 404 | ERR_CANNOTSENDTOCHAN | Pas dans le canal |
| 461 | ERR_NEEDMOREPARAMS | Pas de message |

**Exemple :**
```
Client → Serveur : PRIVMSG #general :Hello everyone!
→ Tous les membres du canal #general reçoivent :
:alice!alice@127.0.0.1 PRIVMSG #general :Hello everyone!
```

---

### 4.7 NOTICE

**Syntaxe :** `NOTICE <target> :<message>`

**Préconditions :** Client enregistré.

**Comportement :** Similaire à PRIVMSG mais sans réponse automatique d'erreur. Utilisé pour les messages automatisés.

**Codes d'erreur :** Aucun (silencieux sur erreur).

---

### 4.8 QUIT

**Syntaxe :** `QUIT [:message]`

**Préconditions :** Aucune.

**Comportement :** Déconnecte le client. Envoie un message de quit à tous les canaux dont le client est membre.

**Exemple :**
```
Client → Serveur : QUIT :Gone to have lunch
→ Tous les membres des canaux de alice reçoivent :
:alice!alice@127.0.0.1 QUIT :Gone to have lunch
```

---

### 4.9 PING

**Syntaxe :** `PING <token>`

**Préconditions :** Aucune.

**Comportement :** Répond immédiatement par PONG avec le même token.

**Exemple :**
```
Client → Serveur : PING abc123
Serveur → Client : :localhost PONG localhost :abc123
```

---

### 4.10 KICK

**Syntaxe :** `KICK #<channel> <nick> [:reason]`

**Préconditions :** Client enregistré, opérateur du canal.

**Comportement :** Exclut un utilisateur du canal.

| Code | Nom | Condition |
|------|-----|-----------|
| 461 | ERR_NEEDMOREPARAMS | Paramètres manquants |
| 403 | ERR_NOSUCHCHANNEL | Canal inexistant |
| 442 | ERR_NOTONCHANNEL | Client pas dans le canal |
| 482 | ERR_CHANOPRIVSNEEDED | Pas opérateur |
| 441 | ERR_USERNOTINCHANNEL | Utilisateur pas dans le canal |

**Exemple :**
```
Client (opérateur) → Serveur : KICK #general bob :Get out!
→ Tous les membres reçoivent :
:alice!alice@127.0.0.1 KICK #general bob :Get out!
```

---

### 4.11 INVITE

**Syntaxe :** `INVITE <nick> #<channel>`

**Préconditions :** Client enregistré, membre du canal.

**Comportement :** Invite un utilisateur. Si le canal est en mode +i, seul un opérateur peut inviter.

| Code | Nom | Condition |
|------|-----|-----------|
| 461 | ERR_NEEDMOREPARAMS | Paramètres manquants |
| 401 | ERR_NOSUCHNICK | Utilisateur inexistant |
| 403 | ERR_NOSUCHCHANNEL | Canal inexistant |
| 442 | ERR_NOTONCHANNEL | Client pas dans le canal |
| 443 | ERR_USERONCHANNEL | Utilisateur déjà dans le canal |
| 482 | ERR_CHANOPRIVSNEEDED | Canal +i et pas opérateur |
| 341 | RPL_INVITING | Confirmation d'invitation |

**Exemple :**
```
Client → Serveur : INVITE bob #general
Serveur → Client : :localhost 341 alice bob #general
→ bob reçoit :
:alice!alice@127.0.0.1 INVITE bob #general
```

---

### 4.12 TOPIC

**Syntaxe :** `TOPIC #<channel> [:<topic>]`

**Préconditions :** Client enregistré, membre du canal.

**Comportement :** Sans paramètre, affiche le topic. Avec paramètre, définit le topic si le mode +t n'est pas actif ou si le client est opérateur.

| Code | Nom | Condition |
|------|-----|-----------|
| 461 | ERR_NEEDMOREPARAMS | Pas de paramètre |
| 403 | ERR_NOSUCHCHANNEL | Canal inexistant |
| 442 | ERR_NOTONCHANNEL | Client pas dans le canal |
| 482 | ERR_CHANOPRIVSNEEDED | Mode +t et pas opérateur |
| 332 | RPL_TOPIC | Topic actuel |
| 331 | RPL_NOTOPIC | Pas de topic |

**Exemple :**
```
Client → Serveur : TOPIC #general
Serveur → Client : :localhost 332 alice #general :Welcome to #general

Client (opérateur) → Serveur : TOPIC #general :New topic
→ Tous les membres reçoivent :
:alice!alice@127.0.0.1 TOPIC #general :New topic
```

---

### 4.13 MODE

**Syntaxe :** `MODE #<channel> [+/-modes] [params]`

**Préconditions :** Client enregistré, opérateur du canal pour modifier.

**Comportement :** Affiche ou modifie les modes du canal.

| Code | Nom | Condition |
|------|-----|-----------|
| 461 | ERR_NEEDMOREPARAMS | Pas de paramètre |
| 403 | ERR_NOSUCHCHANNEL | Canal inexistant |
| 482 | ERR_CHANOPRIVSNEEDED | Pas opérateur |
| 324 | RPL_CHANNELMODEIS | Mode actuel |

**Exemple :**
```
Client → Serveur : MODE #general
Serveur → Client : :localhost 324 alice #general +t

Client (opérateur) → Serveur : MODE #general +i
→ Tous les membres reçoivent :
:alice!alice@127.0.0.1 MODE #general +i
```

---

## 5. Modes de canal — référence

| Mode | Paramètre | Description | Qui peut activer |
|------|-----------|-------------|------------------|
| +i | non | Invite-only : seul un opérateur peut inviter | Opérateur |
| -i | non | Supprime le mode invite-only | Opérateur |
| +t | non | Topic restreint : seul un opérateur peut le changer | Opérateur |
| -t | non | Topic libre pour tous les membres | Opérateur |
| +k | oui (clé) | Définit la clé du canal | Opérateur |
| -k | non | Supprime la clé | Opérateur |
| +o | oui (nick) | Donne le statut opérateur | Opérateur |
| -o | oui (nick) | Retire le statut opérateur | Opérateur |
| +l | oui (limite) | Limite le nombre de membres | Opérateur |
| -l | non | Supprime la limite | Opérateur |

**Impact sur JOIN :**
- +i : Vérifie si le client est invité (liste _invited)
- +k : Vérifie que la clé fournie correspond
- +l : Refuse si le nombre de membres == limite

**Impact sur TOPIC :**
- +t : Seuls les opérateurs peuvent changer le topic

---

## 6. Codes numériques IRC — référence

| Code | Nom constant | Message par défaut | Contexte |
|------|---------------|---------------------|----------|
| 001 | RPL_WELCOME | :Welcome to the IRC Network | Enregistrement |
| 002 | RPL_YOURHOST | :Your host is &lt;server&gt; | Enregistrement |
| 003 | RPL_CREATED | :This server was created &lt;date&gt; | Enregistrement |
| 004 | RPL_MYINFO | &lt;server&gt; &lt;version&gt; &lt;usermodes&gt; &lt;chanmodes&gt; | Enregistrement |
| 315 | RPL_ENDOFWHO | :End of WHO list | WHO |
| 324 | RPL_CHANNELMODEIS | &lt;channel&gt; &lt;modes&gt; | MODE |
| 331 | RPL_NOTOPIC | &lt;channel&gt; :No topic is set | TOPIC/JOIN |
| 332 | RPL_TOPIC | &lt;channel&gt; :&lt;topic&gt; | TOPIC/JOIN |
| 341 | RPL_INVITING | &lt;channel&gt; &lt;nick&gt; | INVITE |
| 352 | RPL_WHOREPLY | voir format | WHO |
| 353 | RPL_NAMREPLY | = &lt;channel&gt; :&lt;names&gt; | JOIN |
| 366 | RPL_ENDOFNAMES | &lt;channel&gt; :End of /NAMES list | JOIN |
| 401 | ERR_NOSUCHNICK | &lt;nick&gt; :No such nick/channel | PRIVMSG, INVITE |
| 403 | ERR_NOSUCHCHANNEL | &lt;channel&gt; :No such channel | JOIN, PART, MODE, etc. |
| 404 | ERR_CANNOTSENDTOCHAN | Cannot send to channel | PRIVMSG |
| 431 | ERR_NONICKNAMEGIVEN | :No nickname given | NICK |
| 432 | ERR_ERRONEUSNICKNAME | &lt;nick&gt; :Erroneous nickname | NICK |
| 433 | ERR_NICKNAMEINUSE | &lt;nick&gt; :Nickname is already in use | NICK |
| 441 | ERR_USERNOTINCHANNEL | &lt;nick&gt; &lt;channel&gt; :They aren't on that channel | KICK |
| 442 | ERR_NOTONCHANNEL | &lt;channel&gt; :You're not on that channel | PART, TOPIC, INVITE |
| 443 | ERR_USERONCHANNEL | &lt;nick&gt; &lt;channel&gt; :is already on channel | INVITE |
| 461 | ERR_NEEDMOREPARAMS | &lt;command&gt; :Not enough parameters | Toutes commandes |
| 462 | ERR_ALREADYREGISTERED | :You may not reregister | PASS, USER |
| 464 | ERR_PASSWDMISMATCH | :Password incorrect | PASS |
| 471 | ERR_CHANNELISFULL | &lt;channel&gt; :Cannot join channel (+l) | JOIN |
| 473 | ERR_INVITEONLYCHAN | &lt;channel&gt; :Cannot join channel (+i) | JOIN |
| 475 | ERR_BADCHANNELKEY | &lt;channel&gt; :Cannot join channel (+k) | JOIN |
| 482 | ERR_CHANOPRIVSNEEDED | &lt;channel&gt; :You're not channel operator | KICK, TOPIC, MODE |

---

## 7. Guide de test

### 7.1 Test de connexion basique avec nc

```bash
# Terminal 1 : Démarrer le serveur
./ircserv 6667 password

# Terminal 2 : Connecter avec nc
nc localhost 6667
PASS password
NICK testuser
USER testuser 0 * :Test User
# Réponse attendue :
:localhost 001 testuser :Welcome to the IRC Network testuser!testuser@127.0.0.1
:localhost 002 testuser :Your host is localhost
:localhost 003 testuser :This server was created ...
:localhost 004 testuser localhost 1.0 + :
```

### 7.2 Test données partielles (cas obligatoire du sujet)

```bash
# Démarrer le serveur
./ircserv 6667 password

# Connecter avec nc utilisant CRLF
nc -C 127.0.0.1 6667

# Envoyer des données PARTIELLEMENT (Ctrl+D = EOF)
PASS pass   # Taper "PASS pass" puis Ctrl+D
             # Le serveur ne répond PAS encore (pas de \r\n)

# Le buffer du serveur contient : "PASS pass"

MAN          # Taper "MAN" puis Ctrl+D
             # Le buffer contient : "PASS passMAN"

D            # Taper "D" puis Entrée
             # Le buffer contient : "PASS passMAND\r\n"
             # Le serveur traite maintenant "PASS pass\r\n"

# Maintenant le serveur répond
NICK alice
USER alice 0 * :Alice
# Réponse attendue
```

### 7.3 Scénario de test complet avec irssi

```bash
# Démarrer le serveur
./ircserv 6667 password

# Dans irssi :
/connect localhost 6667 password mynick
/join #test
/msg #test Hello from irssi!
/topic #test :New topic
/mode #test +i
/mode #test +k secretkey
/invite otheruser #test
/kick #test otheruser :Get out
/quit
```

### 7.4 Cas limites obligatoires

1. **Connexion sans PASS**
   ```
   nc localhost 6667
   NICK test
   USER test 0 * :Test
   # Réponse : le client n'est pas "authenticated", les commandes sont ignorées
   ```

2. **NICK déjà pris**
   ```
   Client A : PASS p / NICK alice / USER a 0 * :A
   Client B : PASS p / NICK alice / USER b 0 * :B
   # Client B reçoit : 433 alice alice :Nickname is already in use
   ```

3. **JOIN canal invite-only sans invitation**
   ```
   Client A (opérateur) : MODE #secret +i
   Client B : JOIN #secret
   # Client B reçoit : 473 #secret :Cannot join channel (+i)
   ```

4. **KICK par un non-opérateur**
   ```
   Client A : JOIN #test
   Client B : JOIN #test
   Client B : KICK #test A
   # Client B reçoit : 482 #test :You're not channel operator
   ```

5. **MODE avec paramètre manquant**
   ```
   Client (opérateur) : MODE #test +k
   # Le serveur doit gérer gracefully (ignorer ou erreur)
   ```

6. **Déconnexion brutale (Ctrl+C)**
   ```
   # Client connecté, dans un canal
   # Ctrl+C côté client
   # Le serveur détecte recv() = 0, appelle removeClient()
   # Le client est retiré du canal, les autres membres sont notifiés
   ```

7. **Envoi de données partielles**
   - Tester avec nc en envoyant caractère par caractère
   - Vérifier que le serveur ne traite qu'après \r\n complet

---

## 8. Glossaire IRC

| Terme | Définition |
|-------|------------|
| fd (file descriptor) | Entier représentant un fichier ou socket ouvert |
| pollfd | Structure utilisée par poll() pour surveiller un fd |
| POLLIN | Flag indiquant que des données sont disponibles en lecture |
| O_NONBLOCK | Flag indiquant qu'une opération I/O ne doit pas bloquer |
| EAGAIN | Erreur indiquant "réessayez" en mode non-bloquant |
| trailing | Dernier paramètre d'un message IRC, peut contenir des espaces (précédé de :) |
| prefix | Préfixe optionnel d'un message IRC (format: nick!user@host) |
| numeric reply | Réponse numérique du serveur (001-004, 331-366, 401-482, etc.) |
| channel operator | Utilisateur avec droits d'administration sur un canal |
| registered client | Client ayant completed l'enregistrement (PASS + NICK + USER) |
| authenticated client | Client ayant validé le mot de passe (PASS correct) |
| mode | Paramètre optionnel modifiant le comportement d'un canal ou utilisateur |
| topic | Sujet/titre d'un canal IRC |
| invite list | Liste des utilisateurs invités sur un canal en mode +i |
| channel key | Mot de passe requis pour rejoindre un canal (mode +k) |

---

## 15. Suite de tests

### 15.1 Exécution des tests

```bash
./test_irc.sh
```

Le serveur doit être en cours d'exécution sur le port 6667 avec le mot de passe "pass".

### 15.2 Résultats des tests

| Test | Résultat | Notes |
|------|----------|-------|
| JOIN without register | ✅ | Retourne 451 comme attendu |
| USER without NICK | ❌ | Retourne 451 (conforme RFC, test incorrect) |
| Full register | ✅ | Retourne 001 |
| Wrong PASS | ✅ | Retourne 464 |
| Double USER | ✅ | Retourne 462 |
| Double NICK (allowed) | ✅ | Fonctionne |
| JOIN normal | ✅ | Retourne 353 |
| JOIN invalid channel | ✅ | Retourne 403 |
| JOIN twice | ✅ | Ignore JOIN répété |
| PART valid | ✅ | Envoie PART au client |
| PART not in channel | ✅ | Retourne 442 |
| Multi-client JOIN | ✅ | Fonctionne |
| PRIVMSG no target | ✅ | Retourne 461 |
| PRIVMSG no message | ✅ | Retourne 461 |
| PRIVMSG unknown nick | ✅ | Retourne 401 |
| PRIVMSG not in channel | ❌ | Retourne 403 (canal supprimé-auto) |
| MODE not operator | ❌ | First user devient opérateur |
| MODE +i | ✅ | Envoie MODE au client |
| Invite-only block | ✅ | Retourne 473 |
| KICK basic | ✅ | Envoie KICK au client |
| KICK non-op | ❌ | First user devient opérateur |
| Weird spacing | ❌ | Parse message avec espaces multiples |
| QUIT | ✅ | Envoie QUIT au client |
| WHO | ✅ | Retourne 352 |
| PING/PONG | ✅ | Retourne PONG |
| Nick collision | ✅ | Gère les collisions |
| Channel delete auto | ✅ | Recrée le canal |
| Spam JOIN/PART | ✅ | Pas de crash |

**Total: 23/28 tests passent**

### 15.3 Corrections appliquées

1. **Channel::addClient()** - Le premier utilisateur rejoint devient opérateur du canal

2. **CommandHandler::PART** - Envoie le message PART au client qui l'émet (en plus du broadcast)

3. **CommandHandler::MODE** - Envoie le changement de mode au client qui l'émet

4. **CommandHandler::KICK** - Envoie le message KICK au client qui l'émet

5. **CommandHandler::QUIT** - Envoie le message QUIT au client avant déconnexion

6. **Channel auto-delete** - Supprime le canal quand le dernier utilisateur le quitte
