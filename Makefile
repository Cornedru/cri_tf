NAME = ircserv
CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iinc
SRC_DIR = src
OBJ_DIR = obj

CLIENT_SRCS = Client.cpp
CHANNEL_SRCS = Channel.cpp
MESSAGE_SRCS = Message.cpp
COMMAND_SRCS = CommandHandler.cpp
SERVER_SRCS = Server.cpp
MAIN_SRCS = main.cpp

CLIENT_OBJS = $(addprefix $(OBJ_DIR)/, $(CLIENT_SRCS:.cpp=.o))
CHANNEL_OBJS = $(addprefix $(OBJ_DIR)/, $(CHANNEL_SRCS:.cpp=.o))
MESSAGE_OBJS = $(addprefix $(OBJ_DIR)/, $(MESSAGE_SRCS:.cpp=.o))
COMMAND_OBJS = $(addprefix $(OBJ_DIR)/, $(COMMAND_SRCS:.cpp=.o))
SERVER_OBJS = $(addprefix $(OBJ_DIR)/, $(SERVER_SRCS:.cpp=.o))
MAIN_OBJS = $(addprefix $(OBJ_DIR)/, $(MAIN_SRCS:.cpp=.o))

OBJS = $(CLIENT_OBJS) $(CHANNEL_OBJS) $(MESSAGE_OBJS) $(COMMAND_OBJS) $(SERVER_OBJS) $(MAIN_OBJS)

all: $(NAME)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all
