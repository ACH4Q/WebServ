#include "../includes/Client.hpp"
#include <unistd.h>

Client::Client() : fd(-1), lastActivity(std::time(NULL)), state(READING_HEADER), config(NULL) {}

Client::Client(int f, struct sockaddr_in a, const ServerConfig& c) : fd(f), addr(a), lastActivity(std::time(NULL)), state(READING_HEADER), config(&c) {}

Client::Client(const Client &copy)
{
    *this = copy;
}

Client &Client::operator=(const Client &assign)
{
    if (this != &assign)
    {
        this->fd = assign.fd;
        this->addr = assign.addr;
        this->lastActivity = assign.lastActivity;
        this->state = assign.state;
        this->readBuffer = assign.readBuffer;
        this->writeBuffer = assign.writeBuffer;
        this->config = assign.config;
    }
    return *this;
}

Client::~Client() {}

void Client::updateTime()
{
    lastActivity = std::time(NULL);
}

bool Client::hasTimedOut(int seconds) const
{
    return (std::difftime(std::time(NULL), lastActivity) > seconds);
}

void Client::reset() {
    state = READING_HEADER;
    readBuffer.clear();
    writeBuffer.clear();
    // request.clear();
    // response.clear();
}