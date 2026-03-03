#include "../includes/Socket.hpp"
#include "../includes/HttpReq.hpp"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h> // <-- Added for poll()

void printRequest(const HttpRequest& req) {
    std::cout << "\n=== RECEIVED FROM POSTMAN ===" << std::endl;
    std::cout << "Method: " << req.getMethod() << std::endl;
    std::cout << "Path:   " << req.getPath() << std::endl;
    std::cout << "State:  " << req.getState() << " (4 = Finished)" << std::endl;
    
    std::cout << "Headers:" << std::endl;
    std::map<std::string, std::string> headers = req.getHeaders();
    for (std::map<std::string, std::string>::iterator it = headers.begin(); it != headers.end(); ++it) {
        std::cout << "  [" << it->first << "] -> [" << it->second << "]" << std::endl;
    }
    std::cout << "Body:   [" << req.getBody() << "]" << std::endl;
    std::cout << "=============================\n" << std::endl;
}

int main()
{
    try {
        Socket serverSocket;
        serverSocket.create(); // Set to O_NONBLOCK under the hood
        serverSocket.bind(8080);
        serverSocket.listen();
        
        std::cout << "[*] Listening on http://localhost:8080" << std::endl;
        std::cout << "[*] Waiting for Postman to connect..." << std::endl;

        // 1. We use poll() to wait until Postman actually connects
        struct pollfd serverPoll;
        serverPoll.fd = serverSocket.getFd();
        serverPoll.events = POLLIN;
        poll(&serverPoll, 1, -1); // -1 means wait indefinitely

        int clientFd = serverSocket.accept();
        if (clientFd < 0) {
            std::cerr << "Failed to accept connection." << std::endl;
            return 1;
        }

        std::cout << "[*] Postman connected! Waiting for data..." << std::endl;

        // 2. We use poll() again to wait until Postman sends the text
        struct pollfd clientPoll;
        clientPoll.fd = clientFd;
        clientPoll.events = POLLIN;
        poll(&clientPoll, 1, -1);

        // Read the data from Postman
        char buffer[4096];
        int bytesRead = recv(clientFd, buffer, sizeof(buffer), 0);
        
        if (bytesRead > 0) {
            std::string rawData(buffer, bytesRead);
            
            HttpRequest req;
            req.parse(rawData);
            printRequest(req);
        }

        // Send a dummy response so Postman shows a success message
        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 22\r\nContent-Type: text/plain\r\n\r\nParser test successful!";
        send(clientFd, response.c_str(), response.length(), 0);

        close(clientFd);
        serverSocket.close();
        std::cout << "[*] Test complete. Shutting down." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}


// epoll