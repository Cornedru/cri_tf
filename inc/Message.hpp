#ifndef MESSAGE_HPP
# define MESSAGE_HPP

# include <string>
# include <vector>

class Message {
private:
    std::string _prefix;
    std::string _command;
    std::vector<std::string> _params;

public:
    Message();
    ~Message();

    const std::string& getPrefix() const;
    const std::string& getCommand() const;
    const std::vector<std::string>& getParams() const;

    void setPrefix(const std::string& prefix);
    void setCommand(const std::string& command);
    void addParam(const std::string& param);

    static Message parse(const std::string& raw);
    std::string toString() const;
};

#endif