#include "../includes/Socket.hpp"

Socket::Socket() : sockfd(-1), port(0) {}

Socket::~Socket()
{
    close();
}

void Socket::create()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        throw std::runtime_error("Error: socket creation failed");
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("Error: fcntl failed");
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

void Socket::bind(int p)
{
    port = p;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("Error: bind failed");
}

void Socket::listen()
{
    if (::listen(sockfd, 100) < 0)
        throw std::runtime_error("Error: listen failed");
}

int Socket::accept()
{
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd = ::accept(sockfd, (struct sockaddr *)&clientAddr, &addrLen);
    if (clientFd >= 0)
    {
        fcntl(clientFd, F_SETFL, O_NONBLOCK);
    }
    return clientFd;
}

int Socket::getFd() const
{
    return sockfd;
}

void Socket::close()
{
    if (sockfd != -1)
    {
        ::close(sockfd);
        sockfd = -1;
    }
}