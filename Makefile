NAME        = webserv
CC          = c++
FLAGS       = -Wall -Wextra -Werror -std=c++98
INCLUDES    = -I includes/
SRCS        = $(wildcard srcs/*.cpp)
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