#include "HttpResponse.hpp"
#include "HttpReq.hpp"
#include "Router.hpp"
#include <string>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <ctime>

const std::string& HttpResponse::getStatusLine() const
{
    return status_line;
}

const std::string& HttpResponse::getResponseBody() const
{
    return response_body;
}

const std::map<std::string, std::string>& HttpResponse::getResponseHeaders() const
{
    return response_headers;
}

void HttpResponse::setStatusLine(int code)
{
    status_code = code;

    switch (code)
    {
        case 200:
            status_line = "HTTP/1.1 200 OK\r\n";
            break;
        case 301:
            status_line = "HTTP/1.1 301 Moved Permanently\r\n";
            break;
        case 302:
            status_line = "HTTP/1.1 302 Found\r\n";
            break;
        case 400:
            status_line = "HTTP/1.1 400 Bad Request\r\n";
            break;
        case 403:
            status_line = "HTTP/1.1 403 Forbidden\r\n";
            break;
        case 404:
            status_line = "HTTP/1.1 404 Not Found\r\n";
            break;
        case 405:
            status_line = "HTTP/1.1 405 Method Not Allowed\r\n";
            break;
        case 500:
            status_line = "HTTP/1.1 500 Internal Server Error\r\n";
            break;
        case 501:
            status_line = "HTTP/1.1 501 Not Implemented\r\n";
            break;
        case 503:
            status_line = "HTTP/1.1 503 Service Unavailable\r\n";
            break;
        default:
            status_line = "HTTP/1.1 500 Internal Server Error\r\n";
            status_code = 500;
            break;
    }
}

void HttpResponse::decideStatus(const HttpRequest& req, const std::string& path, bool serverError)
{
    int code;
    struct stat fileStat;
    if (req.getMethod() != "GET" && req.getMethod() != "POST" && req.getMethod() != "DELETE")
        code = 405;
    else
    {
        if (stat(path.c_str(), &fileStat) != 0)
            code = 404;
        else if (!S_ISREG(fileStat.st_mode))
            code = 403;
        else
        {
            int fd = open(path.c_str(), O_RDONLY);
            if (fd == -1)
                code = 403;
            else
            {
                if (serverError)
                    code = 500;
                else
                    code = 200;
            }
            close(fd);
        }
    }
    setStatusLine(code);
    fileSize = fileStat.st_size;
}


void HttpResponse::setResponseBody(std::string path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
    {
        response_body = "Error opening file";
        return;
    }
    char buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
        response_body.append(buffer, bytesRead);
    std::ostringstream oss;
    oss << response_body.size();
    content_length = oss.str();
    close(fd);
}

void HttpResponse::setResponseHeaders(std::string path)
{
    std::string contentType = "application/octet-stream";
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        std::string extension = path.substr(dotPos + 1);
        if (extension == "html")
            contentType = "text/html";
        else if (extension == "css")
            contentType = "text/css";
        else if (extension == "js")
            contentType = "application/javascript";
        else if (extension == "jpg" || extension == "jpeg")
            contentType = "image/jpeg";
        else if (extension == "png")
            contentType = "image/png";
        else if (extension == "gif")
            contentType = "image/gif";
    }
    response_headers["Content-Type"] = contentType;
    response_headers["Content-Length"] = content_length;
    response_headers["Connection"] = "close";
    response_headers["Server"] = "webserv";
}

void HttpResponse::generateResponse(const HttpRequest& req, RouteResult& routeResult)
{
    decideStatus(req, routeResult.finalPath, !routeResult.isAllowed);
    if (fileSize < 1024 *1024){
        setResponseBody(routeResult.finalPath);
        setResponseHeaders(routeResult.finalPath);
    }
    
    // ├─ 4. Set headers
    // │      ├─ Content-Type (based on file extension)
    // │      ├─ Content-Length (if small file)
    // │      ├─ Transfer-Encoding: chunked (if streaming)
    // │      ├─ Connection: close
    // │      └─ Server: webserv
    // │
    // ├─ 5. Prepare the body
    // │      ├─ Small file → read all into memory
    // │      └─ Large file → don’t read all at once, prepare for streaming
    // │
    // └─ 6. Response is ready to send
    //        ├─ Small file → send in one go
    //        └─ Large file → send in chunks from file
}

std::string handleDelete(const RouteResult& route)
{
    if (!route.isAllowed)
    {
        return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    if (route.isDirectory)
    {
        return "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    if (access(route.finalPath.c_str(), F_OK) != 0)
    {
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    if (access(route.finalPath.c_str(), W_OK) != 0) 
    {
        return "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    if (unlink(route.finalPath.c_str()) == 0)
    {
        std::cout << "[DELETE] Successfully removed: " << route.finalPath << std::endl;
        return "HTTP/1.1 204 No Content\r\nConnection: keep-alive\r\n\r\n";
    } else
    {
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
}

std::string handlePost(const HttpRequest& req, const RouteResult& route)
{
    if (!route.isAllowed)
    {
        return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    if (req.getBody().length() > route.location.maxBodySize)
    {
        return "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    std::string targetPath = route.finalPath;
    struct stat s;
    if (stat(targetPath.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
    {
        if (targetPath[targetPath.length() - 1] != '/')
        {
            targetPath += "/";
        }
        std::ostringstream filename;
        filename << "upload_" << time(NULL) << ".bin";
        targetPath += filename.str();
    }
    std::ofstream outFile(targetPath.c_str(), std::ios::binary);
    // if (!outFile.is_open())
    // {
    //     std::cerr << "[POST] Error: Could not open " << targetPath << " for writing. Check folder permissions." << std::endl;
    //     return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    // }
    // outFile.write(req.getBody().data(), req.getBody().length());
    // outFile.close();
    //SENDI L9LWA
    std::cout << "[POST] Successfully created file: " << targetPath << " (" << req.getBody().length() << " bytes)" << std::endl;
    return "HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
}