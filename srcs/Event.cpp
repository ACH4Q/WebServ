#include "Event.hpp"

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
        bool flag = false;
        std::vector<int> readyFds = epollManager.wait(-1);
        int fd;
        for (size_t i = 0; i < readyFds.size(); ++i) {
            fd = readyFds[i];
            if (fd == manager.getSockets()[i]->getFd()){
                flag = true;
                break;
            }
        }
        if (flag){
            std::cout << "New connection on one of the listening sockets!" << std::endl;
            if ((_clientFd = accept(fd, NULL, NULL)) == -1) {
                std::cerr << "Failed to accept new connection" << std::endl;
            } else {
                int flags = fcntl(_clientFd, F_GETFL, 0);
                fcntl(_clientFd, F_SETFL, flags | O_NONBLOCK);
                epollManager.ctrl(_clientFd, EPOLLIN, EPOLL_CTL_ADD);
                std::cout << "Accepted new connection with fd: " << _clientFd << std::endl;
            }
        }
        else {
            std::cout << "Data ready to read on client fd: " << fd << std::endl;
        }
    }
}