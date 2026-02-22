#include "../includes/HttpReq.hpp"
#include <sstream>
#include <algorithm>
#include <cstdlib>

HttpRequest::HttpRequest() : state(Request_Line), contentLength(0) {}

HttpRequest::~HttpRequest() {}


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