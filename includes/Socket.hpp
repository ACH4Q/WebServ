#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

class Socket
{
    private:
        int                 sockfd;
        struct sockaddr_in  addr;
        int                 port;
    public:
        Socket();
        ~Socket();
        void    create();
        void    bind(int port);
        void    listen();
        int     accept();
        int     getFd() const;
        void    close();
};

#endif