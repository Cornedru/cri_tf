# ft_irc - UML Diagrams (Complete)

> Copiez tout le contenu ci-dessous dans [Mermaid Live Editor](https://mermaid.live/) pour visualiser

---

## UML Class Diagram - Complete Architecture

```mermaid
classDiagram
    class Server {
        -int _port
        -string _password
        -vector~pollfd~ _pollFds
        -map~int, Client*~ _clients
        -map~string, Channel*~ _channels
        -CommandHandler* _commandHandler
        +Server(port: int, password: string)
        +~Server()
        +run(): void
        -initNetwork(): void
        -acceptNewClient(): void
        -handleClientData(fd: int): void
        -removeClient(fd: int): void
        -findClient(nickname: string): Client*
        -findChannel(name: string): Channel*
    }
    
    class CommandHandler {
        -Server* _server
        +CommandHandler(server: Server*)
        +handleCommand(client: Client*, message: Message): void
        -handleRegistration(client: Client*, message: Message): void
        -handleJOIN(client: Client*, params: vector~string~): void
        -handlePART(client: Client*, params: vector~string~): void
        -handlePRIVMSG(client: Client*, params: vector~string~): void
        -handleNOTICE(client: Client*, params: vector~string~): void
        -handleMODE(client: Client*, params: vector~string~): void
        -handleTOPIC(client: Client*, params: vector~string~): void
        -handleKICK(client: Client*, params: vector~string~): void
        -handleINVITE(client: Client*, params: vector~string~): void
        -handleQUIT(client: Client*, message: string): void
        -handleNICK(client: Client*, params: vector~string~): void
        -handleUSER(client: Client*, params: vector~string~): void
        -handlePASS(client: Client*, params: vector~string~): void
        -handleWHO(client: Client*, params: vector~string~): void
        -handlePING(client: Client*, params: vector~string~): void
    }
    
    class Client {
        -int _fd
        -string _readBuffer
        -string _writeBuffer
        -string _nickname
        -string _username
        -string _hostname
        -string _servername
        -string _realname
        -bool _isRegistered
        -bool _isAuthenticated
        -vector~string~ _joinedChannels
        -vector~string~ _invitedChannels
        +Client(fd: int)
        +~Client()
        +getFd(): int
        +getNickname(): string
        +setNickname(nick: string): void
        +getUsername(): string
        +setUsername(user: string): void
        +getHostname(): string
        +setHostname(host: string): void
        +getRealname(): string
        +setRealname(real: string): void
        +isRegistered(): bool
        +setRegistered(reg: bool): void
        +isAuthenticated(): bool
        +setAuthenticated(auth: bool): void
        +appendReadBuffer(data: string): void
        +extractCommand(): string
        +queueData(data: string): void
        +getWriteData(): string
        +clearWriteBuffer(): void
        +addJoinedChannel(channel: string): void
        +removeJoinedChannel(channel: string): void
        +getJoinedChannels(): vector~string~
    }
    
    class Channel {
        -string _name
        -string _topic
        -string _password
        -int _userLimit
        -bool _inviteOnly
        -bool _topicRestricted
        -bool _hasPassword
        -map~string, Client*~ _members
        -map~string, Client*~ _operators
        -vector~string~ _invited
        +Channel(name: string)
        +~Channel()
        +getName(): string
        +getTopic(): string
        +setTopic(topic: string, client: Client*): void
        +getPassword(): string
        +hasPassword(): bool
        +checkPassword(pass: string): bool
        +setPassword(pass: string): void
        +addMember(client: Client*, key: string): bool
        +removeMember(client: Client*): void
        +isMember(client: Client*): bool
        +isOperator(client: Client*): bool
        +addOperator(client: Client*): void
        +removeOperator(client: Client*): void
        +invite(client: Client*): void
        +isInvited(client: Client*): bool
        +getMembers(): vector~Client*~
        +broadcast(message: string, sender: Client*): void
        +isInviteOnly(): bool
        +setInviteOnly(invite: bool): void
        +isTopicRestricted(): bool
        +setTopicRestricted(restricted: bool): void
        +getUserLimit(): int
        +setUserLimit(limit: int): void
        +isFull(): bool
    }
    
    class Message {
        -string _raw
        -string _prefix
        -string _command
        -vector~string~ _parameters
        -string _trailing
        +Message()
        +Message(raw: string)
        +parse(): void
        +getPrefix(): string
        +getCommand(): string
        +getParameters(): vector~string~
        +getTrailing(): string
        +hasTrailing(): bool
        +toString(): string
        +fromClient(client: Client*): void
    }
    
    Server "1" *-- "*" Client : manages
    Server "1" *-- "*" Channel : manages
    Server "1" --> "1" CommandHandler : uses
    CommandHandler "1" --> "1" Server : uses
    CommandHandler "1" --> "*" Client : handles
    CommandHandler "1" --> "*" Channel : handles
    Client "*" -- "*" Channel : joins
    Channel "1" -- "*" Client : contains
    CommandHandler "1" --> "1" Message : parses
    
    class ClientRegistration {
        <<enumeration>>
        UNREGISTERED
        WAIT_NICK
        WAIT_USER
        REGISTERED
        ACTIVE
    }
    
    class ChannelMode {
        <<enumeration>>
        INVITE_ONLY = 'i'
        TOPIC_RESTRICTED = 't'
        PASSWORD = 'k'
        OPERATOR = 'o'
        USER_LIMIT = 'l'
    }
    
    class IRCCommand {
        <<enumeration>>
        PASS
        NICK
        USER
        JOIN
        PART
        PRIVMSG
        NOTICE
        TOPIC
        MODE
        KICK
        INVITE
        QUIT
        WHO
        PING
        PONG
    }
```

---

## UML Use Case Diagram

```mermaid
graph TD
    subgraph Actor["Actor"]
        U[User]
        O[Operator]
        A[Admin]
    end
    
    subgraph UseCases["ft_irc Use Cases"]
        UC1[Connect to Server]
        UC2[Authenticate with Password]
        UC3[Set Nickname]
        UC4[Set Username]
        UC5[Join Channel]
        UC6[Leave Channel]
        UC7[Send Channel Message]
        UC8[Send Private Message]
        UC9[Change Topic]
        UC10[Invite User to Channel]
        UC11[Kick User from Channel]
        UC12[Set Channel Mode]
        UC13[Set Channel Password]
        UC14[Set User Limit]
        UC15[Become Channel Operator]
        UC16[Transfer Operator]
        UC17[PING/PONG Keepalive]
        UC18[Query Users]
        UC19[Disconnect]
    end
    
    U --> UC1
    U --> UC2
    U --> UC3
    U --> UC4
    U --> UC5
    U --> UC6
    U --> UC7
    U --> UC8
    U --> UC9
    U --> UC17
    U --> UC18
    U --> UC19
    
    O --> UC10
    O --> UC11
    O --> UC12
    O --> UC13
    O --> UC14
    O --> UC15
    O --> UC16
    
    A --> UC1
    A --> UC2
    A --> UC3
    A --> UC4
```

---

## UML Sequence Diagram - Full Connection Flow

```mermaid
sequenceDiagram
    participant User as User
    participant IRC as IRC Client
    participant Server as ft_irc Server
    participant DB as Client/Channel Store
    participant Ch as Channel
    
    Note over User,Ch: PHASE 1: Connection
    User->>IRC: Launch client
    IRC->>Server: TCP Connect :6667
    Server->>Server: socket()
    Server->>Server: setsockopt(SO_REUSEADDR)
    Server->>Server: bind(:6667)
    Server->>Server: listen()
    Server->>Server: accept()
    Server->>DB: Create Client object
    DB-->>Server: Client*
    Server-->>IRC: TCP Ack
    
    Note over User,Ch: PHASE 2: Registration
    IRC->>Server: PASS mypassword
    Server->>Server: Validate password
    alt Password correct
        Server->>DB: Mark authenticated
    else Password wrong
        Server-->>IRC: 464 ERR_PASSWDMISMATCH
    end
    
    IRC->>Server: NICK john
    Server->>Server: Check nickname
    alt Nickname available
        Server->>DB: Set nickname
    else Nickname taken
        Server-->>IRC: 433 ERR_NICKNAMEINUSE
    end
    
    IRC->>Server: USER john pc :John Doe
    Server->>Server: Validate user info
    Server->>DB: Set username/realname
    Server->>DB: Mark registered
    Server-->>IRC: 001 RPL_WELCOME
    Server-->>IRC: 002 RPL_YOURHOST
    Server-->>IRC: 003 RPL_CREATED
    Server-->>IRC: 004 RPL_MYINFO
    
    Note over User,Ch: PHASE 3: Channel Operations
    IRC->>Server: JOIN #general
    Server->>DB: Find or create channel
    Server->>Ch: Add client as member
    Server->>Ch: Add client as operator (first joiner)
    Server-->>IRC: :john JOIN #general
    Server->>DB: Update client state
    
    IRC->>Server: TOPIC #general :Welcome!
    Server->>Ch: Set topic
    Server->>Ch: Check topic restriction
    alt User is operator or no restriction
        Server-->>IRC: TOPIC #general :Welcome!
    else Topic restricted
        Server-->>IRC: 482 ERR_CHANOPRIVSNEEDED
    end
    
    Note over User,Ch: PHASE 4: Messaging
    IRC->>Server: PRIVMSG #general :Hello!
    Server->>Ch: Get all members
    loop For each member except sender
        Server->>IRC: :john PRIVMSG #general :Hello!
    end
    
    IRC->>Server: PRIVMSG jane :Hi!
    Server->>DB: Find jane
    alt jane found
        Server-->>jane: :john PRIVMSG jane :Hi!
    else jane not found
        Server-->>IRC: 401 ERR_NOSUCHNICK
    end
    
    Note over User,Ch: PHASE 5: Operator Actions
    IRC->>Server: INVITE jane #general
    Server->>Ch: Check operator
    alt Sender is operator
        Server->>Ch: Add jane to invited
        Server-->>jane: :john INVITE jane #general
        Server-->>IRC: 341 RPL_INVITING
    else Not operator
        Server-->>IRC: 482 ERR_CHANOPRIVSNEEDED
    end
    
    IRC->>Server: MODE #general +i
    Server->>Ch: Set invite-only
    Server-->>IRC: MODE #general +i
    
    Note over User,Ch: PHASE 6: Disconnection
    IRC->>Server: QUIT :Goodbye
    Server->>DB: Remove from all channels
    Server->>DB: Close socket
    Server->>DB: Delete client
    Server-->>IRC: Connection closed
```

---

## UML Sequence Diagram - Message Processing

```mermaid
sequenceDiagram
    participant C as Client
    participant S as Server
    participant Buff as Client Buffer
    participant Msg as Message Parser
    participant Cmd as CommandHandler
    participant Ch as Channel
    participant Other as Other Clients
    
    C->>S: recv() partial data
    S->>Buff: appendReadBuffer(data)
    Buff->>Buff: Store in buffer
    
    S->>Buff: extractCommand()
    Buff->>Buff: Search for \r\n
    alt Command incomplete
        Buff-->>S: return NULL
    else Command complete
        Buff-->>S: return "PRIVMSG #ch :Hello"
    end
    
    S->>Msg: Message(raw)
    Msg->>Msg: Parse
    Msg->>Msg: Extract prefix, command, params, trailing
    Msg-->>S: Message object
    
    S->>S: Validate format
    alt Invalid format
        S->>C: ERR_UNKNOWNCOMMAND
    else Valid format
        S->>S: Check registration
        alt Not registered
            S->>C: ERR_NOTREGISTERED
        else Registered
            S->>Cmd: handleCommand(client, message)
            
            Cmd->>Cmd: Route by command
            Cmd->>Ch: Find channel #ch
            alt Channel not found
                Cmd->>C: ERR_NOSUCHCHANNEL
            else Channel found
                Cmd->>Ch: Check membership
                alt Not member
                    Cmd->>C: ERR_NOTONCHANNEL
                else Member
                    Ch->>Ch: Broadcast message
                    loop For each member
                        S->>Other: PRIVMSG #ch :Hello
                    end
                    S->>C: Message processed
                end
            end
        end
    end
```

---

## UML State Machine Diagram - Client Lifecycle

```mermaid
stateDiagram-v2
    [*] --> TCP_CONNECTING: new connection
    
    state TCP_CONNECTING {
        [*] --> CONNECTING
        CONNECTING --> CONNECTED: accept() OK
    }
    
    TCP_CONNECTING --> UNREGISTERED: fd created
    
    state UNREGISTERED {
        [*] --> WAIT_PASS_OR_NICK
        
        WAIT_PASS_OR_NICK --> WAIT_PASS_OR_NICK: Invalid PASS
        WAIT_PASS_OR_NICK --> WAIT_NICK: Valid PASS / no PASS required
        
        WAIT_NICK --> WAIT_NICK: Invalid NICK
        WAIT_NICK --> WAIT_USER: Valid NICK
        WAIT_NICK --> WAIT_NICK: NICK already taken (433)
        
        WAIT_USER --> WAIT_USER: Incomplete USER
        WAIT_USER --> WAIT_USER: Missing required params
        WAIT_USER --> REGISTERED: Complete USER
        
        note right of WAIT_PASS_OR_NICK
            Commands:
            • PASS
            • NICK
            • USER
            • PING
        end note
        
        note right of WAIT_NICK
            Commands:
            • PASS
            • NICK
            • USER
            • PING
        end note
        
        note right of WAIT_USER
            Commands:
            • PASS
            • NICK
            • USER
            • PING
        end note
    }
    
    UNREGISTERED --> REGISTERED: NICK + USER complete
    
    state REGISTERED {
        [*] --> READY
        
        READY --> READY: JOIN #channel
        READY --> READY: PART #channel
        READY --> READY: PRIVMSG/NOTICE
        READY --> READY: TOPIC
        READY --> READY: WHO
        READY --> READY: PING/PONG
        
        READY --> CHANNEL_OPERATOR: MODE +o received
        CHANNEL_OPERATOR --> READY: Lose operator
        
        READY --> INVITING: INVITE user
        INVITING --> READY: Done
        
        READY --> INVITE_ONLY_SET: MODE +i
        INVITE_ONLY_SET --> READY: MODE -i
        
        READY --> PASSWORD_PROTECTED: MODE +k
        PASSWORD_PROTECTED --> READY: MODE -k
        
        note right of READY
            Commands:
            • JOIN/PART
            • PRIVMSG/NOTICE
            • TOPIC
            • MODE
            • KICK
            • INVITE
            • WHO
            • QUIT
            • PING/PONG
        end note
    }
    
    REGISTERED --> CLIENT_ACTIVE: Welcome messages sent (001-004)
    
    REGISTERED --> DISCONNECTING: QUIT
    CLIENT_ACTIVE --> DISCONNECTING: QUIT
    CLIENT_ACTIVE --> DISCONNECTING: Connection lost
    
    DISCONNECTING --> [*]: Close fd, delete Client
```

---

## UML Component Diagram

```mermaid
graph TB
    subgraph Client_Layer["Client Applications"]
        IRC1[irssi]
        IRC2[WeeChat]
        IRC3[HexChat]
        IRC4[netcat]
    end
    
    subgraph Server_Process["ft_irc Server Process"]
        subgraph Network_Layer["Network Layer"]
            SOCK[Socket<br/>Management]
            POLL[poll Event<br/>Loop]
            NONBLOCK[O_NONBLOCK<br/>I/O]
        end
        
        subgraph Core_Components["Core Components"]
            SERVER[Server<br/>Class]
            CMD[CommandHandler<br/>Class]
            MSG[Message<br/>Class]
        end
        
        subgraph Data_Model["Data Model"]
            CLIENTS[Client<br/>Manager]
            CHANNELS[Channel<br/>Manager]
            BUFFER[Buffer<br/>Management]
        end
    end
    
    subgraph Command_Handlers["Command Handlers"]
        REG[Registration<br/>PASS/NICK/USER]
        CHAN[Channel<br/>JOIN/PART/MODE]
        MSG[Message<br/>PRIVMSG/NOTICE]
        OPER[Operator<br/>KICK/INVITE]
        UTIL[Utility<br/>WHO/PING/QUIT]
    end
    
    IRC1 --> SOCK
    IRC2 --> SOCK
    IRC3 --> SOCK
    IRC4 --> SOCK
    
    SOCK --> POLL
    POLL --> NONBLOCK
    NONBLOCK --> SERVER
    
    SERVER --> CLIENTS
    SERVER --> CHANNELS
    SERVER --> CMD
    SERVER --> BUFFER
    
    CMD --> MSG
    CMD --> REG
    CMD --> CHAN
    CMD --> MSG
    CMD --> OPER
    CMD --> UTIL
    
    CLIENTS --> |"Client*"| CHANNELS
    CHANNELS --> |"Channel*"| CLIENTS
```

---

## UML Deployment Diagram

```mermaid
graph LR
    subgraph Client_Host["Client Machine"]
        subgraph Client_OS["OS: Linux/Windows/macOS"]
            IRC_APP[IRC Client<br/>irssi/WeeChat/HexChat]
            NETCAT[netcat]
        end
    end
    
    subgraph Server_Host["Server Machine"]
        subgraph Server_OS["OS: Linux"]
            subgraph ft_irc_Process["ft_irc Process"]
                MAIN[main.cpp]
                
                subgraph Server_Component["Server Component"]
                    INIT[Initialization]
                    LOOP[Event Loop]
                end
                
        subgraph Network["Network"]
            SOCKET[TCP Socket<br/>:6667]
            POLL[ poll ]
        end
                
                subgraph Classes["Classes"]
                    SERVER_C[Server]
                    CLIENT_C[Client]
                    CHANNEL_C[Channel]
                    MESSAGE_C[Message]
                    CMD_C[CommandHandler]
                end
            end
            
            KERNEL[Linux Kernel]
        end
    end
    
    IRC_APP --> |TCP:6667| SOCKET
    NETCAT --> |TCP:6667| SOCKET
    SOCKET --> POLL
    POLL --> LOOP
    LOOP --> SERVER_C
    SERVER_C --> CLIENT_C
    SERVER_C --> CHANNEL_C
    SERVER_C --> CMD_C
    CMD_C --> MESSAGE_C
    
    SOCKET <--> KERNEL
```

---

## UML Package Diagram

```mermaid
graph TD
    subgraph ft_irc_Package["ft_irc"]
        
        subgraph src_package["src/"]
            MAIN[main.cpp]
            
            SERVER_PKG[Server Package]
            SERVER_PKG --> SERVER_CPP[Server.cpp]
            SERVER_PKG --> CLIENT_CPP[Client.cpp]
            SERVER_PKG --> CHANNEL_CPP[Channel.cpp]
            SERVER_PKG --> MESSAGE_CPP[Message.cpp]
            SERVER_PKG --> CMD_CPP[CommandHandler.cpp]
        end
        
        subgraph inc_package["inc/"]
            SERVER_HPP[Server.hpp]
            CLIENT_HPP[Client.hpp]
            CHANNEL_HPP[Channel.hpp]
            MESSAGE_HPP[Message.hpp]
            CMD_HPP[CommandHandler.hpp]
        end
        
        BUILD[Makefile]
    end
    
    EXTERNAL["External"]
    EXTERNAL --> |POSIX| SERVER_PKG
    EXTERNAL --> |C++98| SERVER_PKG
    
    MAIN --> SERVER_HPP
    MAIN --> SERVER_CPP
    
    SERVER_CPP --> CLIENT_HPP
    SERVER_CPP --> CHANNEL_HPP
    SERVER_CPP --> CMD_HPP
    
    CMD_CPP --> MESSAGE_HPP
    
    BUILD --> SERVER_CPP
    BUILD --> CLIENT_CPP
    BUILD --> CHANNEL_CPP
    BUILD --> MESSAGE_CPP
    BUILD --> CMD_CPP
```

---

Pour visualiser **tout d'un coup** :
1. Ouvrez [Mermaid Live Editor](https://mermaid.live/)
2. Copiez **tout le contenu** de ce fichier
3. Cliquez sur "Sync"
