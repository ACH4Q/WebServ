#include "HttpResponse.hpp"

void HttpResponse::setStatusLine(std::string &path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
    {
        status_line = "HTTP/1.1 404 Not Found\r\n";
        status_code = 404;
    }
    else
    {
        status_line = "HTTP/1.1 200 OK\r\n";
        status_code = 200;
    }
}

const std::string& HttpResponse::getStatusLine() const
{
    return status_line;
}