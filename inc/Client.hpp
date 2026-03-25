#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>
# include <vector>

class Client {
private:
    int _fd;
    std::string _hostname;
    std::string _nick;
    std::string _username;
    std::string _realname;
    std::string _buffer;
    bool _registered;
    bool _passOk;

public:
    Client(int fd, const std::string& hostname);
    ~Client();

    int getFd() const;
    const std::string& getHostname() const;
    const std::string& getNick() const;
    const std::string& getUsername() const;
    const std::string& getPrefix() const;
    bool isRegistered() const;
    bool hasPassword() const;

    void setNick(const std::string& nick);
    void setUsername(const std::string& username);
    void setRealname(const std::string& realname);
    void setRegistered();
    void setPasswordOk();

    void appendToBuffer(const std::string& data);
    std::vector<std::string> extractMessages();
    void sendMsg(const std::string& msg);
};

#endif