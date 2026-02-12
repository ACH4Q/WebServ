#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <netinet/in.h>
#include <string>
#include <ctime>
#include "Config.hpp"
#include <iostream>


enum ClientState
{
    READING_HEADER,
    READING_BODY,
    WRITING_RESPONSE,
    KEEP_ALIVE,
    CLOSE
};

class Client
{
    public:
        int                 fd;
        struct sockaddr_in  addr;
        time_t              lastActivity;
        ClientState         state;
        std::string         readBuffer;
        std::string         writeBuffer;
        const ServerConfig* config; 
        Client();
        Client(int fd, struct sockaddr_in addr, const ServerConfig& config);
        Client(const Client &copy);
        Client &operator=(const Client &assign);
        ~Client();
        void    updateTime();
        bool    hasTimedOut(int seconds) const;
        void    reset();
};

#endif