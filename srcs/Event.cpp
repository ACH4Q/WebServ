#include "../includes/Event.hpp"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include "../includes/HttpReq.hpp"
#include "../includes/HttpResponse.hpp"
#include "../includes/Router.hpp"
#include "../includes/AutoIndex.hpp"


Event::Event() {}

Event::~Event() {}

void Event::run(SocketManager& manager, EpollManager& epollManager) {
    for (size_t i = 0; i < manager.getSockets().size(); ++i)
    {
        std::cout << "http://localhost:"
                  << manager.getSockets()[i]->getPort()
                  << std::endl;
    }

    while (true) {
        std::vector<int> readyFds = epollManager.wait(-1);

        for (size_t i = 0; i < readyFds.size(); ++i) {
            int fd = readyFds[i];

            bool isListeningSocket = false;
            size_t serverIndex = 0;

            for (size_t j = 0; j < manager.getSockets().size(); ++j) {
                if (fd == manager.getSockets()[j]->getFd()) {
                    isListeningSocket = true;
                    serverIndex = j;
                    break;
                }
            }

            if (isListeningSocket)
            {
                if ((_clientFd = accept(fd, NULL, NULL)) == -1) {
                    std::cerr << "Failed to accept new connection" << std::endl;
                }
                else
                {
                    int flags = fcntl(_clientFd, F_GETFL, 0);
                    fcntl(_clientFd, F_SETFL, flags | O_NONBLOCK);
                    epollManager.ctrl(_clientFd, EPOLLIN, EPOLL_CTL_ADD);
                    clientServerIndex[_clientFd] = serverIndex;
                }
            }
            else
            {
                if (HttpResponse::hasPendingLargeTransfer(fd))
                {
                    bool done = HttpResponse::continueLargeTransfer(fd);
                    if (done)
                    {
                        epollManager.ctrl(fd, 0, EPOLL_CTL_DEL);
                        close(fd);
                        requests.erase(fd);
                        clientServerIndex.erase(fd);
                    }
                    continue;
                }
                char buffer[4096];
                int bytesRead = recv(fd, buffer, sizeof(buffer), 0);
                if (bytesRead > 0)
                {
                    std::string rawData(buffer, bytesRead);
                    requests[fd].parse(rawData);
                    if (requests[fd].getState() == Request_Finished)
                    {
                        std::cout << "Successfully parsed request: "
                                  << requests[fd].getMethod() << " "
                                  << requests[fd].getPath() << " "
                                  << requests[fd].getBodyFilename() << " "
                                  << requests[fd].getVersion()
                                  << std::endl;
                        Router routeResult;
                        size_t index = clientServerIndex[fd];
                        RouteResult result = routeResult.resolve(requests[fd],*manager.getSockets()[index]->getServer());
                        AutoIndex autoIndex;
                        std::string autoIndexContent = autoIndex.generate(result.finalPath, requests[fd].getPath());
                        HttpResponse response;
                        response.generateResponse(requests[fd], result, fd, autoIndexContent);
                        if (HttpResponse::hasPendingLargeTransfer(fd))
                        {
                            epollManager.ctrl(fd, EPOLLOUT, EPOLL_CTL_MOD);
                            requests.erase(fd);
                        }
                        else
                        {
                            epollManager.ctrl(fd, 0, EPOLL_CTL_DEL);
                            close(fd);
                            requests.erase(fd);
                            clientServerIndex.erase(fd);
                        }
                    }
                }
                else
                {
                    epollManager.ctrl(fd, 0, EPOLL_CTL_DEL);
                    close(fd);
                    requests.erase(fd);
                    clientServerIndex.erase(fd);
                }
            }
        }
    }
}