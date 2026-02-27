#include "../includes/HttpReq.hpp"
#include <sstream>
#include <algorithm>
#include <cstdlib>

HttpRequest::HttpRequest() : state(Request_Line), contentLength(0), errorCode(0) {}

HttpRequest::~HttpRequest() {}

void HttpRequest::parse(std::string &rawBuffer)
{
    storage += rawBuffer;
    rawBuffer.clear();

    while (true)
    {
        if (state == Request_Line || state == Request_Headers)
        {
            size_t pos = storage.find("\r\n");
            if (pos == std::string::npos) 
                break;
            std::string line = storage.substr(0, pos);
            storage.erase(0, pos + 2);

            if (state == Request_Line)
            {
                if (line.empty())
                    continue; 
                if (parseRequestLine(line)) 
                    state = Request_Headers;
                else
                {
                    errorCode = 400;
                    state = Request_Finished;
                }
            } else if (state == Request_Headers)
            {
                if (line.empty())
                {
                    if (headers.count("Transfer-Encoding") && headers["Transfer-Encoding"] == "chunked")
                    {
                        state = Request_Chunked;
                    } else if (headers.count("Content-Length"))
                    {
                        contentLength = std::strtoul(headers["Content-Length"].c_str(), NULL, 10);
                        if (contentLength == 0) 
                            state = Request_Finished;
                        else state = Request_Body;
                    } else
                    {
                        state = Request_Finished;
                    }
                } else
                {
                    parseHeaders(line);
                }
            }
        } 
        else if (state == Request_Body)
        {
            parseBody();
            break; 
        }
        else if (state == Request_Chunked)
        {
            processChunk(storage);
            break;
        }
        if (state == Request_Finished)
            break;
    }
}

bool HttpRequest::parseRequestLine(std::string &line)
{
    std::stringstream ss(line);
    if (!(ss >> method >> path >> version))
        return false;
    if (method != "GET" && method != "POST" && method != "DELETE")
        return false;
    return !path.empty() && !version.empty();
}

void HttpRequest::parseHeaders(std::string &line)
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
}

void HttpRequest::parseBody()
{
    size_t needed = contentLength - body.size();
    size_t toRead = std::min(needed, storage.size());
    body += storage.substr(0, toRead);
    storage.erase(0, toRead);
    if (body.size() >= contentLength) 
        state = Request_Finished;
}

void HttpRequest::processChunk(std::string &storage)
{
    while (true)
    {
        size_t pos = storage.find("\r\n");
        if (pos == std::string::npos)
            return;
        std::string hexSize = storage.substr(0, pos);
        unsigned long chunkSize = std::strtoul(hexSize.c_str(), NULL, 16);
        if (chunkSize == 0)
        {
            if (storage.find("\r\n\r\n", pos) != std::string::npos)
            {
                storage.erase(0, pos + 4);
                state = Request_Finished;
            }
            return;
        }
        if (storage.size() < pos + 2 + chunkSize + 2)
            return;
        body.append(storage.substr(pos + 2, chunkSize));
        storage.erase(0, pos + 2 + chunkSize + 2);
    }
}

void HttpRequest::reset()
{
    method.clear();
    path.clear();
    version.clear();
    headers.clear();
    body.clear();
    storage.clear();
    state = Request_Line;
    errorCode = 0;
}

const std::string& HttpRequest::getMethod() const
{ 
    return method;
}

const std::string& HttpRequest::getPath() const
{ return
    path;
}

RequestParseState HttpRequest::getState() const
{
    return
    state;
}