#include "Channel.hpp"
#include "Client.hpp"

Channel::Channel(const std::string& name) : _name(name), _userLimit(0) {}

Channel::~Channel() {}

const std::string& Channel::getName() const { return _name; }
const std::string& Channel::getTopic() const { return _topic; }
const std::set<Client*>& Channel::getClients() const { return _clients; }

bool Channel::hasClient(Client* client) const {
    return _clients.find(client) != _clients.end();
}

bool Channel::hasOperator(Client* client) const {
    return _operators.find(client) != _operators.end();
}

bool Channel::hasPassword() const { return !_password.empty(); }

void Channel::setTopic(const std::string& topic) { _topic = topic; }
void Channel::setPassword(const std::string& password) { _password = password; }
void Channel::setUserLimit(size_t limit) { _userLimit = limit; }

void Channel::addClient(Client* client) { _clients.insert(client); }
void Channel::removeClient(Client* client) {
    _clients.erase(client);
    _operators.erase(client);
}
void Channel::addOperator(Client* client) { _operators.insert(client); }
void Channel::removeOperator(Client* client) { _operators.erase(client); }

void Channel::broadcast(const std::string& message, Client* except) {
    for (std::set<Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (*it != except) {
            (*it)->sendMsg(message);
        }
    }
}