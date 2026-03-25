#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <string>
# include <set>
# include <map>

class Client;

class Channel {
private:
    std::string _name;
    std::string _topic;
    std::set<Client*> _clients;
    std::set<Client*> _operators;
    std::string _password;
    size_t _userLimit;

public:
    Channel(const std::string& name);
    ~Channel();

    const std::string& getName() const;
    const std::string& getTopic() const;
    const std::set<Client*>& getClients() const;
    bool hasClient(Client* client) const;
    bool hasOperator(Client* client) const;
    bool hasPassword() const;

    void setTopic(const std::string& topic);
    void setPassword(const std::string& password);
    void setUserLimit(size_t limit);

    void addClient(Client* client);
    void removeClient(Client* client);
    void addOperator(Client* client);
    void removeOperator(Client* client);
    void broadcast(const std::string& message, Client* except = NULL);
};

#endif