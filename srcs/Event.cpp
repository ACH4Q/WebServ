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
        std::vector<int> readyFds = epollManager.wait(-1);
        for (size_t i = 0; i < readyFds.size(); ++i) {
            int fd = readyFds[i];
            std::cout << "Ready fd: " << fd << std::endl;
            // Handle events for the ready file descriptors
            // This is where you would accept new connections, read/write data, etc.
        }
    }
}