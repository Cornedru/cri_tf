#include "Client.hpp"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

Client::Client(int fd, const std::string& hostname)
    : _fd(fd), _hostname(hostname), _registered(false), _passOk(false) {}

Client::~Client() {}

int Client::getFd() const { return _fd; }
const std::string& Client::getHostname() const { return _hostname; }
const std::string& Client::getNick() const { return _nick; }
const std::string& Client::getUsername() const { return _username; }
const std::string& Client::getPrefix() const {
    static std::string prefix;
    if (!_nick.empty()) {
        prefix = _nick + "!" + _username + "@" + _hostname;
    } else {
        prefix = _hostname;
    }
    return prefix;
}
bool Client::isRegistered() const { return _registered; }
bool Client::hasPassword() const { return _passOk; }

void Client::setNick(const std::string& nick) { _nick = nick; }
void Client::setUsername(const std::string& username) { _username = username; }
void Client::setRealname(const std::string& realname) { _realname = realname; }
void Client::setRegistered() { _registered = true; }
void Client::setPasswordOk() { _passOk = true; }

void Client::appendToBuffer(const std::string& data) { _buffer += data; }

std::vector<std::string> Client::extractMessages() {
    std::vector<std::string> messages;
    size_t pos;
    while ((pos = _buffer.find("\r\n")) != std::string::npos) {
        messages.push_back(_buffer.substr(0, pos));
        _buffer.erase(0, pos + 2);
    }
    return messages;
}

void Client::sendMsg(const std::string& msg) {
    send(_fd, msg.c_str(), msg.size(), 0);
}