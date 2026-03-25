#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <string>
# include <map>
# include <vector>
# include <netinet/in.h>
# include <poll.h>

# include "Client.hpp"
# include "Channel.hpp"
# include "Message.hpp"
# include "CommandHandler.hpp"

class Server {
private:
    int _port;
    std::string _password;
    int _sockfd;
    std::map<int, Client*> _clients;
    std::map<std::string, Channel*> _channels;
    std::vector<struct pollfd> _pollfds;

    Server(const Server& other);
    Server& operator=(const Server& other);

public:
    Server(int port, const std::string& password);
    ~Server();

    void run();
    void acceptClient();
    void removeClient(int fd);
    void dispatch(const Message& msg, Client& client);
    void handleClientData(int fd);

    Client* getClient(int fd) const;
    Client* getClientByNick(const std::string& nick) const;
    Channel* getChannel(const std::string& name) const;
    const std::string& getPassword() const;
    const std::string& getHostname() const;

    void addChannel(Channel* channel);
    void removeChannel(const std::string& name);
    const std::map<std::string, Channel*>& getChannels() const;

    void sendToAll(const std::string& msg, Client* except = NULL);
};

#endif
