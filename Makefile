NAME        = webserv
CC          = c++ 
FLAGS       = -Wall -Wextra -Werror -std=c++98
INCLUDES    = -I includes/
SRCS        = srcs/AutoIndex.cpp \
	srcs/Cgi.cpp \
	srcs/Client.cpp \
	srcs/ConfigParser.cpp \
	srcs/EpollManager.cpp \
	srcs/Event.cpp \
	srcs/HttpReq.cpp \
	srcs/HttpResponse.cpp \
	srcs/ListeningSocket.cpp \
	srcs/main.cpp \
	srcs/ManageServers.cpp \
	srcs/Router.cpp \
	srcs/SessionManager.cpp
OBJS        = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(FLAGS) $(INCLUDES) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CC) $(FLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re