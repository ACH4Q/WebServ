#ifndef LISTENINGSOCKET_HPP
#define LISTENINGSOCKET_HPP

#include "Config.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>

class ListeningSocket {
private:
    int _fd;
    ServerConfig* _server;

public:
    ListeningSocket(ServerConfig* server);
    ~ListeningSocket();
    void createSocket();
    void setReuseAddr();
    void bindSocket();
    void startListening(int backlog);
    void setNonBlocking();
    int getFd() const;
};


#endif