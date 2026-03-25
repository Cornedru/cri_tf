#ifndef COMMANDHANDLER_HPP
# define COMMANDHANDLER_HPP

class Server;
class Client;
class Message;

class CommandHandler {
public:
    static void execute(const Message& msg, Client& client, Server& server);
};

#endif