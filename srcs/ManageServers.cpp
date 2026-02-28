#include "ManageServers.hpp"

SocketManager::~SocketManager() {
    for (size_t i = 0; i < _listeningSockets.size(); ++i) {
        ListeningSocket* sock = _listeningSockets[i];
        delete sock;
    }
    _listeningSockets.clear();
    closeAllSockets();
}

void SocketManager::addSocket( ListeningSocket *sock) {
    _listeningSockets.push_back(sock);
}

std::vector<ListeningSocket *> SocketManager::getSockets() {
    return _listeningSockets;
}

std::vector<int> SocketManager::getFds() const {
    std::vector<int> fds;
    for (size_t i = 0; i < _listeningSockets.size(); ++i) {
        fds.push_back(_listeningSockets[i]->getFd());
    }
    return fds;
}

void SocketManager::generateListeningSockets(const std::vector<ServerConfig>& servers) {
    for (size_t i = 0; i < servers.size(); ++i) {
        ListeningSocket *sock = new ListeningSocket(const_cast<ServerConfig*>(&servers[i]));
        sock->createSocket();
        sock->setReuseAddr();
        sock->bindSocket();
        sock->startListening(128);
        sock->setNonBlocking();
        addSocket(sock);
    }
}

void SocketManager::closeAllSockets() {
    for (size_t i = 0; i < _listeningSockets.size(); ++i) {
        close(_listeningSockets[i]->getFd());
    }
    _listeningSockets.clear();
}