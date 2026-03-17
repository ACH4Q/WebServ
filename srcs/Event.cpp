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
    for (size_t i = 0; i < manager.getSockets().size(); ++i) {
        std::cout << "http://localhost:" << manager.getSockets()[i]->getPort() << std::endl;
    }

    while (true) {
        processCgiTasks(epollManager);
        std::vector<struct epoll_event> readyEvents = epollManager.wait(10);
        
        for (size_t i = 0; i < readyEvents.size(); ++i) {
            int fd = readyEvents[i].data.fd;
            uint32_t events = readyEvents[i].events;

            bool isListeningSocket = false;
            size_t serverIndex = 0;
            for (size_t j = 0; j < manager.getSockets().size(); ++j) {
                if (fd == manager.getSockets()[j]->getFd()) {
                    isListeningSocket = true;
                    serverIndex = j;
                    break;
                }
            }

            if (isListeningSocket) {
                if ((_clientFd = accept(fd, NULL, NULL)) == -1) {
                    std::cerr << "Failed to accept new connection" << std::endl;
                } else
                {
                    int flags = fcntl(_clientFd, F_GETFL, 0);
                    fcntl(_clientFd, F_SETFL, flags | O_NONBLOCK);
                    epollManager.ctrl(_clientFd, EPOLLIN, EPOLL_CTL_ADD);
                    clientServerIndex[_clientFd] = serverIndex;
                }
            }
            if (!isListeningSocket && (events & EPOLLIN || events & EPOLLOUT))
            {
                if (HttpResponse::hasPendingLargeTransfer(fd)) {
                    bool done = HttpResponse::continueLargeTransfer(fd);
                    if (done) {
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
                        std::cout << "Successfully parsed request For : " << requests[fd].getMethod() << " " << requests[fd].getVersion() << " " << requests[fd].getPath() << std::endl;
                        Router routeResult;
                        size_t index = clientServerIndex[fd];
                        RouteResult result = routeResult.resolve(requests[fd],*manager.getSockets()[index]->getServer());
                        AutoIndex autoIndex;
                        std::string autoIndexContent = autoIndex.generate(result.finalPath, requests[fd].getPath());
                        HttpResponse response;
                        response.generateResponse(requests[fd], result, fd, autoIndexContent);
                        if (response.isCgi) {
                            if (response.cgiPid != -1) {
                                CgiTask task;
                                task.pid = response.cgiPid;
                                task.clientFd = fd;
                                task.outFilename = response.cgiOutFilename;
                                task.startTime = time(NULL);
                                cgiTasks.push_back(task);
                                
                                epollManager.ctrl(fd, 0, EPOLL_CTL_DEL);
                            } else {
                                epollManager.ctrl(fd, 0, EPOLL_CTL_DEL);
                                close(fd);
                                requests.erase(fd);
                                clientServerIndex.erase(fd);
                            }
                        } else if (HttpResponse::hasPendingLargeTransfer(fd))
                        {
                            epollManager.ctrl(fd, EPOLLOUT, EPOLL_CTL_MOD);
                            requests.erase(fd);
                        } else
                        {
                            epollManager.ctrl(fd, 0, EPOLL_CTL_DEL);
                            close(fd);
                            requests.erase(fd);
                            clientServerIndex.erase(fd);
                        }
                        std::cout << "Response processing completed" << std::endl;
                    }
                } else {
                    epollManager.ctrl(fd, 0, EPOLL_CTL_DEL);
                    close(fd);
                    requests.erase(fd);
                    clientServerIndex.erase(fd);
                }
            }
        }
    }
}

void Event::sendCgiResponse(int clientFd, const std::string& outFilename, int statusCode, EpollManager& epollManager) 
{
    (void)epollManager;
    if (statusCode == 504) 
    {
        std::string err = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(clientFd, err.c_str(), err.length(), MSG_NOSIGNAL);
    } 
    else if (statusCode == 502) 
    {
        std::string err = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(clientFd, err.c_str(), err.length(), MSG_NOSIGNAL);
    } 
    else 
    {
        std::ifstream outFile(outFilename.c_str());
        if (outFile) 
        {
            std::stringstream buffer;
            buffer << outFile.rdbuf();
            std::string rawCgiOutput = buffer.str();
            std::string headersPart, bodyPart;
            size_t headerEnd = rawCgiOutput.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                headersPart = rawCgiOutput.substr(0, headerEnd);
                bodyPart = rawCgiOutput.substr(headerEnd + 4);
            } else {
                bodyPart = rawCgiOutput;
            }
            
            std::ostringstream finalResponse;
            finalResponse << "HTTP/1.1 200 OK\r\n";
            if (!headersPart.empty()) finalResponse << headersPart << "\r\n";
            if (headersPart.find("Content-Type:") == std::string::npos) finalResponse << "Content-Type: text/html\r\n";
            finalResponse << "Content-Length: " << bodyPart.length() << "\r\n\r\n" << bodyPart;
            
            std::string responseStr = finalResponse.str();
            send(clientFd, responseStr.c_str(), responseStr.length(), MSG_NOSIGNAL);
        }
    }
    
    std::remove(outFilename.c_str());
    close(clientFd);
    requests.erase(clientFd);
    clientServerIndex.erase(clientFd);
}

void Event::processCgiTasks(EpollManager& epollManager) 
{
    for (std::vector<CgiTask>::iterator it = cgiTasks.begin(); it != cgiTasks.end(); ) 
    {
        int status;
        pid_t res = waitpid(it->pid, &status, WNOHANG); 

        if (res == it->pid)
        {
            int code = 200;
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) 
                code = 502;
            sendCgiResponse(it->clientFd, it->outFilename, code, epollManager);
            it = cgiTasks.erase(it);
        } 
        else if (res == 0)
        {
            time_t elapsed = time(NULL) - it->startTime;

            if (elapsed >= 10)
            {
                kill(it->pid, SIGKILL);
                waitpid(it->pid, &status, 0);
                
                sendCgiResponse(it->clientFd, it->outFilename, 504, epollManager);
                it = cgiTasks.erase(it);
            } 
            else 
            {
                ++it;
            }
        } 
        else
        {
            sendCgiResponse(it->clientFd, it->outFilename, 502, epollManager);
            it = cgiTasks.erase(it);
        }
    }
}