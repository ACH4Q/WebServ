#ifndef EVENT_HPP
#define EVENT_HPP

#include "Client.hpp"
#include "ListeningSocket.hpp"
#include "EpollManager.hpp"
#include "HttpReq.hpp"
#include "ManageServers.hpp"
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <sys/wait.h>
#include <signal.h>

struct CgiTask
{
    pid_t pid;
    int clientFd;
    std::string outFilename;
    time_t startTime;
};

class Event{
    private:
        int _clientFd;
        std::map<int, HttpRequest> requests;
        std::map<int, size_t> clientServerIndex;
        std::vector<CgiTask> cgiTasks;
        void processCgiTasks(EpollManager& epollManager);
        void sendCgiResponse(int clientFd, const std::string& outFilename, int statusCode, EpollManager& epollManager);
    public:
        Event();
        ~Event();
        void run(SocketManager& manager, EpollManager& epollManager);
};



#endif