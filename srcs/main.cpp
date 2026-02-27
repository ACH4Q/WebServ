#include "../includes/ConfigParser.hpp"
#include <iostream>
#include "ManageServers.hpp"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./webserv [config_file]" << std::endl;
        return 1;
    }

    try
    {
        ConfigParser parser;
        parser.parse(argv[1]);
        std::vector<ServerConfig> servers = parser.getServers();
        std::cout << "Successfully parsed config file!" << std::endl;
        std::cout << "Found " << servers.size() << " servers." << std::endl;
        if (!servers.empty())
        {
            SocketManager manager;
            manager.generateListeningSockets(servers);
            std::cout << "Listening sockets created for all servers." << std::endl;
            std::vector<int> fds = manager.getFds();
            for (size_t i = 0; i < servers.size(); ++i)
            {
                std::cout << "Listening on ports: ";
                std::cout << servers[i].port << " (fd: " << fds[i] << ") ";
                std::cout << std::endl;
            }
            // manager.cleanup();
        }
    } catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}