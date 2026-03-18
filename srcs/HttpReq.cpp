#include "../includes/HttpReq.hpp"
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>  
#include <ctime>   
#include <fcntl.h>  
#include <unistd.h> 

HttpRequest::HttpRequest() : bodyFd(-1), bodyBytesWritten(0), state(Request_Line), contentLength(0), errorCode(0), hasCookies(false) {}

HttpRequest::~HttpRequest() 
{
    if (bodyFd != -1) 
    {
        close(bodyFd);
        bodyFd = -1;
    }
}

void HttpRequest::openTempFile()
{
    std::ostringstream ss;
    ss << "./www/html/www/files/uploads" << this << "_" << time(NULL) << ".bin";
    bodyFilename = ss.str();
    
    bodyFd = open(bodyFilename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
    
    if (bodyFd == -1)
    {
        errorCode = 500;
        state = Request_Finished;
    }
    else
    {
        bodyBytesWritten = 0;
    }
}

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
            {
                break;
            }
            
            std::string line = storage.substr(0, pos);
            storage.erase(0, pos + 2);

            if (state == Request_Line)
            {
                if (line.empty())
                {
                    continue; 
                }
                
                if (parseRequestLine(line))
                {
                    state = Request_Headers;
                }
                else 
                {
                    if (errorCode == 0) 
                        errorCode = 400;
                    state = Request_Finished; 
                }
            } 
            else if (state == Request_Headers)
            {
                if (line.empty())
                {
                    if (headers.count("Transfer-Encoding") && headers["Transfer-Encoding"] == "chunked") 
                    {
                        openTempFile();
                        if (state != Request_Finished) 
                        {
                            state = Request_Chunked;
                        }
                    } 
                    else if (headers.count("Content-Length")) 
                    {
                        contentLength = std::strtoul(headers["Content-Length"].c_str(), NULL, 10);
                        
                        if (contentLength == 0)
                        {
                            state = Request_Finished;
                        }
                        else 
                        {
                            openTempFile();
                            if (state != Request_Finished) 
                            {
                                state = Request_Body;
                            }
                        }
                    } 
                    else 
                    {
                        state = Request_Finished;
                    }
                } 
                else 
                {
                    if (!parseHeaders(line)) 
                    {
                        if (errorCode == 0) 
                            errorCode = 400;
                        state = Request_Finished;
                        break;
                    }
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
        {
            break;
        }
    }
}

bool HttpRequest::parseRequestLine(std::string &line)
{
    std::stringstream ss(line);
    std::string extraGarbage;
    if (!(ss >> method >> path >> version))
    {
        errorCode = 400;
        return false;
    }
    if (ss >> extraGarbage)
    {
        errorCode = 400;
        return false;
    }
    if (method != "GET" && method != "POST" && method != "DELETE")
    {
        // errorCode = 501;
        return false;
    }
    if (version != "HTTP/1.1")
    {
        errorCode = 505; 
        return false;
    }
    if (path.empty() || path[0] != '/')
    {
        errorCode = 400;
        return false;
    }
    return true;
}

bool HttpRequest::parseHeaders(std::string &line)
{
    size_t colon = line.find(":");
    if (colon == std::string::npos || colon == 0)
    {
        return false;
    }
    if (line[colon - 1] == ' ' || line[colon - 1] == '\t')
    {
        return false;
    }

    std::string key = line.substr(0, colon);
    bool capitalizeNext = true;
    for (size_t i = 0; i < key.length(); ++i)
    {
        if (key[i] == ' ' || key[i] == '\t')
        {
            return false;
        }
        
        if (capitalizeNext && key[i] >= 'a' && key[i] <= 'z') key[i] -= 32; 
        else if (!capitalizeNext && key[i] >= 'A' && key[i] <= 'Z') key[i] += 32; 
        capitalizeNext = (key[i] == '-');
    }
    
    std::string value = line.substr(colon + 1);
    size_t first = value.find_first_not_of(" \t");
    size_t last = value.find_last_not_of(" \t\r\n");
    
    if (first != std::string::npos && last != std::string::npos && first <= last)
    {
        headers[key] = value.substr(first, (last - first + 1));
        if (key == "Cookie")
        {
            hasCookies = true;
        }
    }
    else 
    {
        headers[key] = "";
    }
    
    return true;
}

void HttpRequest::parseBody()
{
    if (bodyFd == -1)
    {
        return;
    }

    size_t needed = contentLength - bodyBytesWritten;
    size_t toWrite = std::min(needed, storage.size());

    ssize_t written = write(bodyFd, storage.data(), toWrite);
    
    if (written > 0)
    {
        bodyBytesWritten += written;
        storage.erase(0, written); 
    }

    if (bodyBytesWritten >= contentLength) 
    {
        close(bodyFd);
        bodyFd = -1;
        state = Request_Finished;
    }
}

void HttpRequest::processChunk(std::string &storage)
{
    if (bodyFd == -1)
    {
        return;
    }

    while (true)
    {
        size_t pos = storage.find("\r\n");
        
        if (pos == std::string::npos)
        {
            return;
        }
            
        std::string hexSize = storage.substr(0, pos);
        unsigned long chunkSize = std::strtoul(hexSize.c_str(), NULL, 16);
        
        if (chunkSize == 0)
        {
            if (storage.find("\r\n\r\n", pos) != std::string::npos)
            {
                storage.erase(0, pos + 4);
                close(bodyFd);
                bodyFd = -1;
                state = Request_Finished;
            }
            return;
        }
        
        if (storage.size() < pos + 2 + chunkSize + 2)
        {
            return;
        }
            
        ssize_t written = write(bodyFd, storage.data() + pos + 2, chunkSize);
        
        if (written > 0)
        {
            bodyBytesWritten += written;
        }
        
        storage.erase(0, pos + 2 + chunkSize + 2);
    }
}

void HttpRequest::reset()
{
    method.clear();
    path.clear();
    version.clear();
    headers.clear();
    storage.clear();
    
    if (bodyFd != -1) 
    {
        close(bodyFd);
        bodyFd = -1;
    }
        
    if (errorCode != 0 && !bodyFilename.empty()) 
    {
        std::remove(bodyFilename.c_str());
    }
        
    bodyFilename.clear();
    bodyBytesWritten = 0;
    contentLength = 0;
    state = Request_Line;
    errorCode = 0;
    hasCookies = false;
}

const std::string& HttpRequest::getMethod() const 
{ 
    return method; 
}

const std::string& HttpRequest::getPath() const 
{ 
    return path; 
}

RequestParseState HttpRequest::getState() const 
{ 
    return state;
}

const std::string& HttpRequest::getVersion() const 
{ 
    return version; 
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const 
{ 
    return headers; 
}

int HttpRequest::getErrorCode() const
{ 
    return errorCode; 
}

const std::string& HttpRequest::getBodyFilename() const 
{ 
    return bodyFilename; 
}

bool HttpRequest::getHasCookies() const
{
    return hasCookies;
}