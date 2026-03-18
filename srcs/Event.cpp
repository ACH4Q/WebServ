#include "../includes/Event.hpp"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include "../includes/HttpReq.hpp"
#include "../includes/HttpResponse.hpp"
#include "../includes/Router.hpp"
#include "../includes/AutoIndex.hpp"

static std::string reasonPhrase(int statusCode)
{
    switch (statusCode)
    {
        case 502:
            return "Bad Gateway";
        case 504:
            return "Gateway Timeout";
        default:
            return "Internal Server Error";
    }
}

static std::string buildDefaultErrorBody(int statusCode)
{
    std::ostringstream body;
    body << "<!DOCTYPE html>\n"
         << "<html lang=\"en\">\n"
         << "<head><meta charset=\"UTF-8\"><title>" << statusCode << " " << reasonPhrase(statusCode) << "</title></head>\n"
         << "<body>\n"
         << "<h1>" << statusCode << " " << reasonPhrase(statusCode) << "</h1>\n"
         << "<p>The upstream CGI did not respond correctly.</p>\n"
         << "<hr><p>webserv</p>\n"
         << "</body>\n"
         << "</html>\n";
    return body.str();
}


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
                                task.bodyFilename = requests[fd].getBodyFilename();
                                task.startTime = time(NULL);
                                task.routeResult = result;
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

void Event::sendCgiResponse(const CgiTask& task, int statusCode)
{
    if (statusCode == 504 || statusCode == 502)
    {
        std::string body;
        std::map<int, std::string>::const_iterator it = task.routeResult.errorPages.find(statusCode);
        if (it != task.routeResult.errorPages.end())
        {
            std::string errorPagePath = task.routeResult.serverRoot + it->second;
            std::ifstream errorFile(errorPagePath.c_str(), std::ios::in | std::ios::binary);
            if (errorFile)
            {
                std::ostringstream bodyStream;
                bodyStream << errorFile.rdbuf();
                body = bodyStream.str();
            }
        }
        if (body.empty())
            body = buildDefaultErrorBody(statusCode);

        std::ostringstream response;
        response << "HTTP/1.1 " << statusCode << " " << reasonPhrase(statusCode) << "\r\n";
        response << "Server: webserv\r\n";
        response << "Connection: close\r\n";
        response << "Content-Type: text/html\r\n";
        response << "Content-Length: " << body.size() << "\r\n\r\n";
        response << body;

        std::string errorResponse = response.str();
        send(task.clientFd, errorResponse.c_str(), errorResponse.length(), MSG_NOSIGNAL);
    }
    else
    {
        std::ifstream outFile(task.outFilename.c_str());
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
            send(task.clientFd, responseStr.c_str(), responseStr.length(), MSG_NOSIGNAL);
        }
    }
    
    std::remove(task.outFilename.c_str()); 
    if (!task.bodyFilename.empty()) {
        std::remove(task.bodyFilename.c_str());
    }
    close(task.clientFd);
    requests.erase(task.clientFd);
    clientServerIndex.erase(task.clientFd);
}

void Event::processCgiTasks(EpollManager& epollManager) 
{
    (void)epollManager;
    for (std::vector<CgiTask>::iterator it = cgiTasks.begin(); it != cgiTasks.end(); ) 
    {
        int status;
        pid_t res = waitpid(it->pid, &status, WNOHANG); 

        if (res == it->pid)
        {
            int code = 200;
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) 
                code = 502;
            sendCgiResponse(*it, code);
            it = cgiTasks.erase(it);
        } 
        else if (res == 0)
        {
            time_t elapsed = time(NULL) - it->startTime;

            if (elapsed >= 10)
            {
                kill(it->pid, SIGKILL);
                waitpid(it->pid, &status, 0);
                sendCgiResponse(*it, 504);
                it = cgiTasks.erase(it);
            } 
            else 
            {
                ++it;
            }
        } 
        else
        {
            sendCgiResponse(*it, 502);
            it = cgiTasks.erase(it);
        }
    }
}