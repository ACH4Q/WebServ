#include "../includes/HttpReq.hpp"
#include <sstream>
#include <algorithm>
#include <cstdlib>

HttpRequest::HttpRequest() : state(Request_Line), contentLength(0) {}

HttpRequest::~HttpRequest() {}

bool HttpRequest::parseRequestLine(std::string &line)
{
    std::stringstream ss(line);
    ss >> method >> path >> version;
    if (method != "GET" && method != "POST" && method != "DELETE")
        return false;
    return !path.empty() && !version.empty();
}

bool HttpRequest::parseHeaders(std::string &line)
{
    size_t colon = line.find(":");
    if (colon != std::string::npos)
    {
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        size_t first = value.find_first_not_of(" ");
        size_t last = value.find_last_not_of(" ");
        if (first != std::string::npos)
            headers[key] = value.substr(first, (last - first + 1));
    }
    return true;
}

void HttpRequest::parseChunked(std::string &line)
{
    if (line.find("0\r\n\r\n") != std::string::npos)
        state = Request_Finished;
}

std::string HttpRequest::getMethod() const 
{
    return method;
}

std::string HttpRequest::getPath() const
{
    return path;
}

RequestParseState HttpRequest::getState() const
{
    return state;
}

void HttpRequest::parse(std::string &rawBuffer)
{

}
