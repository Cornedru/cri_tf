*This project has been created as part of the 42 curriculum by ndehmej.*

# ft_irc — IRC Server

## Description

ft_irc is a complete Internet Relay Chat (IRC) server implementation in C++98, developed as part of the 42 school curriculum. The server implements the IRC protocol as defined in RFC 1459 and RFC 2812, enabling multiple clients to connect, join channels, and exchange messages in real-time.

The server uses a single `poll()` loop to handle all file descriptors (listening socket and all connected clients) in a non-blocking manner. All I/O operations are performed with `O_NONBLOCK` flags, ensuring the server never blocks while waiting for data. This architecture allows the server to handle thousands of concurrent connections without using threads or fork.

The implementation follows a strict C++98 standard with no external libraries, relying only on standard POSIX system calls for networking. The codebase is organized into five core classes: Server (network initialization and event loop), Client (connection state and buffering), Channel (channel management and modes), Message (IRC message parsing), and CommandHandler (command dispatching and execution).

## Features

- **Connection Management**
  - TCP socket listening with SO_REUSEADDR
  - Non-blocking I/O using O_NONBLOCK
  - Single poll() loop for all file descriptors
  - Graceful client disconnection handling

- **Client Registration**
  - PASS command for password authentication
  - NICK command for nickname selection
  - USER command for user identification
  - Automatic welcome messages (001-004)

- **Channel Management**
  - JOIN/PART commands
  - Automatic channel creation on first join
  - Channel operator status for first joiner
  - Topic management with TOPIC command

- **Messaging**
  - PRIVMSG for direct and channel messages
  - NOTICE for non-reply messages
  - Proper IRC message format with prefix

- **Channel Modes**
  - +i: Invite-only channels
  - +t: Topic restriction to operators
  - +k: Channel key/password
  - +o: Operator status assignment
  - +l: User limit

- **Operator Commands**
  - KICK: Remove users from channels
  - INVITE: Invite users to channels
  - MODE: Modify channel parameters

- **Protocol Support**
  - PING/PONG for connection keepalive
  - WHO for user enumeration
  - QUIT with optional quit message

## Instructions

### Requirements

- **Operating System**: Linux (tested on Ubuntu/Debian)
- **Compiler**: GCC 4.8+ with C++98 support
- **Libraries**: Standard C++ library, POSIX sockets (no external dependencies)

### Compilation

```bash
# Clone the repository and navigate to the project directory
cd ft_irc

# Compile the project
make

# Clean object files
make clean

# Clean everything (including binary)
make fclean

# Rebuild from scratch
make re
```

### Usage

```bash
# Start the server on port 6667 with password "mypassword"
./ircserv 6667 mypassword

# The server will listen on all interfaces (0.0.0.0)
```

### Connecting

**Using nc (netcat):**
```bash
# Connect and register
nc localhost 6667
PASS mypassword
NICK mynick
USER myuser 0 * :My Real Name

# Join a channel
JOIN #general

# Send a message
PRIVMSG #general :Hello everyone!

# Quit
QUIT :Goodbye!
```

**Using irssi:**
```bash
# In irssi:
/connect localhost 6667 mypassword mynick
/join #general
/msg #general Hello!
```

**Using WeeChat:**
```bash
# In WeeChat:
/connect localhost 6667
/set irc.server.localhost.password mypassword
/join #general
```

### Testing Partial Data

The server correctly handles partial data reception, which is a mandatory test case:

```bash
# Start server
./ircserv 6667 password &

# Test with nc -C (using CRLF line endings)
nc -C 127.0.0.1 6667

# Type commands partially:
# Press Ctrl+D after each partial input
COM      (press Ctrl+D)
MAN      (press Ctrl+D)
D        (press Enter)

# The server will NOT process until \r\n is complete
# After "COMMAND\r\n", it will process the command
```

## Project Structure

```
ft_irc/
├── Makefile              # Build system
├── README.md             # This file
├── inc/                  # Header files
│   ├── Client.hpp
│   ├── Server.hpp
│   ├── Channel.hpp
│   ├── Message.hpp
│   └── CommandHandler.hpp
├── src/                  # Source files
│   ├── main.cpp
│   ├── Client.cpp
│   ├── Server.cpp
│   ├── Channel.cpp
│   ├── Message.cpp
│   └── CommandHandler.cpp
└── obj/                  # Object files (generated)
```

## Resources

### Protocol References

- **RFC 1459**: https://tools.ietf.org/html/rfc1459 — Original IRC protocol specification
- **RFC 2812**: https://tools.ietf.org/html/rfc2812 — Modern IRC protocol
- **Modern IRC Documentation**: https://modern.ircdocs.horse/ — Comprehensive IRC protocol guide

### Network Programming

- **Beej's Guide to Network Programming**: https://beej.us/guide/bgnet/ — Essential guide for socket programming
- **poll() man page**: `man poll` — POSIX poll() system call documentation
- **IRC Numerics Reference**: https://www.irc.org/techdocs.html — Complete list of numeric replies

### IRC Clients Used for Testing

- **irssi**: https://irssi.org/ — Terminal-based IRC client
- **WeeChat**: https://weechat.org/ — Extensible CLI chat client
- **HexChat**: https://hexchat.github.io/ — GTK-based IRC client
