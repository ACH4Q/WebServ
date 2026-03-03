#ifndef EVENT_HPP
#define EVENT_HPP

#include "Client.hpp"
#include "ListeningSocket.hpp"
#include "EpollManager.hpp"
#include "ManageServers.hpp"
#include <vector>

class Event{
    private:
        int _clientFd;
    public:
        Event();
        ~Event();
        void run(SocketManager& manager, EpollManager& epollManager);
};



#endif