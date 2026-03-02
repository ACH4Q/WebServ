#include "EpollManager.hpp"


EpollManager::EpollManager() {
    _epollFd = epoll_create1(0);
    if (_epollFd == -1) {
        throw std::runtime_error("Failed to create epoll instance");
    }
}

EpollManager::~EpollManager() {
    if (_epollFd != -1) {
        close(_epollFd);
    }
}

void EpollManager::ctrl(int fd, uint32_t events, int operation) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(_epollFd, operation, fd, &ev) == -1)
        throw std::runtime_error("Failed to control epoll instance");
}

std::vector<int> EpollManager::wait(int timeout) {
    std::vector<int> readyFds;
    struct epoll_event events[1024];
    int numEvents = epoll_wait(_epollFd, events, 1024, timeout);
    if (numEvents == -1)
        throw std::runtime_error("Failed to wait on epoll instance");
    for (int i = 0; i < numEvents; ++i)
        readyFds.push_back(events[i].data.fd);
    return readyFds;
}