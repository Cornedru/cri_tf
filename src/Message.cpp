#include "Message.hpp"
#include <sstream>

Message::Message() {}

Message::~Message() {}

const std::string& Message::getPrefix() const { return _prefix; }
const std::string& Message::getCommand() const { return _command; }
const std::vector<std::string>& Message::getParams() const { return _params; }

void Message::setPrefix(const std::string& prefix) { _prefix = prefix; }
void Message::setCommand(const std::string& command) { _command = command; }
void Message::addParam(const std::string& param) { _params.push_back(param); }

Message Message::parse(const std::string& raw) {
    Message msg;
    std::string s = raw;

    if (s.empty())
        return msg;

    if (s[0] == ':') {
        size_t space = s.find(' ');
        if (space != std::string::npos) {
            msg.setPrefix(s.substr(1, space - 1));
            s = s.substr(space + 1);
        }
    }

    if (s.empty())
        return msg;

    size_t space = s.find(' ');
    if (space == std::string::npos) {
        msg.setCommand(s);
        return msg;
    }

    msg.setCommand(s.substr(0, space));
    s = s.substr(space + 1);

    while (!s.empty()) {
        if (s[0] == ':') {
            msg.addParam(s.substr(1));
            break;
        }
        space = s.find(' ');
        if (space == std::string::npos) {
            msg.addParam(s);
            break;
        }
        msg.addParam(s.substr(0, space));
        s = s.substr(space + 1);
    }

    return msg;
}

std::string Message::toString() const {
    std::string result;
    if (!_prefix.empty())
        result += ":" + _prefix + " ";
    result += _command;
    for (size_t i = 0; i < _params.size(); ++i) {
        if (i == _params.size() - 1 && (_params[i].find(' ') != std::string::npos || _params[i][0] == ':')) {
            result += " :" + _params[i];
        } else {
            result += " " + _params[i];
        }
    }
    return result;
}