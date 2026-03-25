#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Message.hpp"
#include "CommandHandler.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

Server::Server(int port, const std::string& password)
    : _port(port), _password(password), _sockfd(-1) {

    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) {
        throw std::runtime_error("socket() failed");
    }

    int opt = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);

    if (bind(_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(_sockfd);
        throw std::runtime_error("bind() failed");
    }

    if (listen(_sockfd, 10) < 0) {
        close(_sockfd);
        throw std::runtime_error("listen() failed");
    }

    int flags = fcntl(_sockfd, F_GETFL, 0);
    fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);

    struct pollfd pfd;
    pfd.fd = _sockfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollfds.push_back(pfd);
}

Server::~Server() {
    std::map<int, Client*>::iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it) {
        delete it->second;
    }
    std::map<std::string, Channel*>::iterator cit;
    for (cit = _channels.begin(); cit != _channels.end(); ++cit) {
        delete cit->second;
    }
    if (_sockfd >= 0) {
        close(_sockfd);
    }
}

void Server::run() {
    while (true) {
        int n = poll(&_pollfds[0], _pollfds.size(), -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        for (size_t i = 0; i < _pollfds.size(); ++i) {
            if (!(_pollfds[i].revents & POLLIN)) {
                continue;
            }
            if (_pollfds[i].fd == _sockfd) {
                acceptClient();
            } else {
                handleClientData(_pollfds[i].fd);
            }
        }
    }
}

void Server::acceptClient() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int clientFd = accept(_sockfd, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientFd < 0) {
        return;
    }

    int flags = fcntl(clientFd, F_GETFL, 0);
    fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);

    char hostname[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, hostname, INET_ADDRSTRLEN);

    Client* client = new Client(clientFd, std::string(hostname));
    _clients[clientFd] = client;

    struct pollfd pfd;
    pfd.fd = clientFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollfds.push_back(pfd);
}

void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) {
        return;
    }
    Client* client = it->second;
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
    for (size_t i = 0; i < channelsToRemove.size(); ++i) {
        delete _channels[channelsToRemove[i]];
        _channels.erase(channelsToRemove[i]);
    }
    close(fd);
    for (size_t i = 0; i < _pollfds.size(); ++i) {
        if (_pollfds[i].fd == fd) {
            _pollfds.erase(_pollfds.begin() + i);
            break;
        }
    }
    delete client;
    _clients.erase(fd);
}

void Server::dispatch(const Message& msg, Client& client) {
    CommandHandler::execute(msg, client, *this);
}

void Server::handleClientData(int fd) {
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));

    int ret = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (ret <= 0) {
        if (ret < 0 && errno == EAGAIN) {
            return;
        }
        removeClient(fd);
        return;
    }

    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) {
        removeClient(fd);
        return;
    }

    Client* client = it->second;
    client->appendToBuffer(std::string(buffer, ret));

    std::vector<std::string> messages = client->extractMessages();
    for (size_t i = 0; i < messages.size(); ++i) {
        Message msg = Message::parse(messages[i]);
        if (!msg.getCommand().empty()) {
            dispatch(msg, *client);
        }
    }
}

Client* Server::getClient(int fd) const {
    std::map<int, Client*>::const_iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        return it->second;
    }
    return NULL;
}

Client* Server::getClientByNick(const std::string& nick) const {
    std::map<int, Client*>::const_iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNick() == nick) {
            return it->second;
        }
    }
    return NULL;
}

Channel* Server::getChannel(const std::string& name) const {
    std::map<std::string, Channel*>::const_iterator it = _channels.find(name);
    if (it != _channels.end()) {
        return it->second;
    }
    return NULL;
}

const std::string& Server::getPassword() const {
    return _password;
}

const std::string& Server::getHostname() const {
    static std::string hostname = "localhost";
    return hostname;
}

void Server::addChannel(Channel* channel) {
    _channels[channel->getName()] = channel;
}

void Server::removeChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        delete it->second;
        _channels.erase(it);
    }
}

const std::map<std::string, Channel*>& Server::getChannels() const {
    return _channels;
}

void Server::sendToAll(const std::string& msg, Client* except) {
    std::map<int, Client*>::iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second != except) {
            it->second->sendMsg(msg);
        }
    }
}
