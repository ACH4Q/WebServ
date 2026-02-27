#ifndef MANAGESERVERS_HPP
#define MANAGESERVERS_HPP

#include "Config.hpp"
#include "ListeningSocket.hpp"
#include <vector>


class SocketManager {
private:
    std::vector<ListeningSocket> _listeningSockets;

public:
    void addSocket(const ListeningSocket& sock);
    std::vector<ListeningSocket>& getSockets();
    std::vector<int> getFds() const;
    void generateListeningSockets(const std::vector<ServerConfig>& servers);
    void closeAllSockets();
    ~SocketManager();
};


#endif