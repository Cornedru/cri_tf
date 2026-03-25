#include "CommandHandler.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Message.hpp"

void CommandHandler::execute(const Message& msg, Client& client, Server& server) {
    const std::string& cmd = msg.getCommand();
    const std::vector<std::string>& params = msg.getParams();

    if (cmd == "PASS") {
        if (params.empty()) {
            client.sendMsg(":localhost 461 PASS :Not enough parameters");
            return;
        }
        if (client.hasPassword()) {
            client.sendMsg(":localhost 462 :You may not register again");
            return;
        }
        if (params[0] == server.getPassword()) {
            client.setPasswordOk();
        } else {
            client.sendMsg(":localhost 464 :Password incorrect");
        }
    } else if (cmd == "NICK") {
        if (params.empty()) {
            client.sendMsg(":localhost 431 :No nickname given");
            return;
        }
        client.setNick(params[0]);
    } else if (cmd == "USER") {
        if (params.size() < 4) {
            client.sendMsg(":localhost 461 USER :Not enough parameters");
            return;
        }
        client.setUsername(params[0]);
        client.setRealname(params[3]);
        if (client.hasPassword() && !client.getNick().empty()) {
            client.setRegistered();
            client.sendMsg(":localhost 001 " + client.getNick() + " :Welcome to the IRC server");
        }
    } else if (cmd == "PING") {
        client.sendMsg("PONG " + (params.empty() ? "" : params[0]));
    } else if (cmd == "JOIN") {
        if (params.empty()) {
            client.sendMsg(":localhost 461 JOIN :Not enough parameters");
            return;
        }
        const std::string& channelName = params[0];
        server.getChannel(channelName);
    } else if (cmd == "PRIVMSG") {
        if (params.size() < 2) {
            client.sendMsg(":localhost 411 PRIVMSG :No recipient given");
            return;
        }
    } else if (cmd == "QUIT") {
        const std::string& msgText = params.empty() ? "Quit" : params[0];
        client.sendMsg("ERROR :Quit: " + msgText);
    } else if (cmd == "WHO") {
        client.sendMsg(":localhost 315 " + client.getNick() + " :End of WHO list");
    }
}