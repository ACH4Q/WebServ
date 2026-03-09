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
#include <sys/socket.h>

int HttpResponse::check_status_fourhundred(const HttpRequest& req, const RouteResult& routeResult)
{
    if (!routeResult.isAllowed){
        errorOccurred = true;
        return 405;
    }
    if (routeResult.isDirectory && req.getMethod() != "GET")
    {
        errorOccurred = true;
        return 403;
    }
    if (!routeResult.isDirectory && access(routeResult.finalPath.c_str(), F_OK) != 0)
    {
        errorOccurred = true;
        return 404;
    }
    if (req.getMethod() == "POST" && req.getBody().length() > routeResult.location.maxBodySize)
    {
        errorOccurred = true;
        return 413;
    }
    return 0;
}

void HttpResponse::generate_status_code(const HttpRequest& req, const RouteResult& routeResult)
{
    status_code = 0;
    if ((status_code = check_status_fourhundred(req, routeResult)) != 0){
        errorOccurred = true;
        return;
    }
    status_code = 200;
    errorOccurred = false;
}

void HttpResponse::check_error(const HttpRequest& req, const RouteResult& routeResult)
{
    generate_status_code(req, routeResult);
}

void HttpResponse::setStatusLine()
{
    switch (status_code)
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
            break;
    }
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
        else if (extension == "mp4")
            contentType = "video/mp4";
        else if (extension == "pdf")
            contentType = "application/pdf";
    }
    response_headers["Content-Type"] = contentType;
    response_headers["Content-Length"] = content_length;
    response_headers["Connection"] = "close";
    response_headers["Server"] = "webserv";
}

void HttpResponse::Status_file(const RouteResult& routeResult)
{
    struct stat fileStat;
    if (stat(routeResult.finalPath.c_str(), &fileStat) == 0)
    {
        fileSize = fileStat.st_size;
    }
    else
    {
        fileSize = 0;
        content_length = "0";
    }
}

void HttpResponse::set_body(const std::string& path)
{
    int fd = open(path.c_str(), O_RDONLY);
    char buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
        response_body.append(buffer, bytesRead);
    std::ostringstream oss;
    oss << response_body.size();
    content_length = oss.str();
    close(fd);
}

void HttpResponse::write_response()
{
    std::string finalResponse = status_line;
    for (std::map<std::string, std::string>::const_iterator it = response_headers.begin(); it != response_headers.end(); ++it)
        finalResponse += it->first + ": " + it->second + "\r\n";
    finalResponse += "\r\n" + response_body;
    send(_clientFd, finalResponse.c_str(), finalResponse.size(), 0);
}

void HttpResponse::set_directory_autoindex( const std::string& autoIndexContent){
        response_body = autoIndexContent;
        std::string contentType = "text/html";
        std::ostringstream oss;
        oss << response_body.size();
        content_length = oss.str();
        response_headers["Content-Type"] = contentType;
        response_headers["Content-Length"] = content_length;
        response_headers["Connection"] = "close";
        response_headers["Server"] = "webserv";
        write_response();
}

void HttpResponse::send_small_files(const RouteResult& routeResult, const std::string& autoIndexContent)
{
    if (routeResult.isDirectory && routeResult.location.autoindex)
        set_directory_autoindex(autoIndexContent);
    else
    {
        setResponseHeaders(routeResult.finalPath);
        set_body(routeResult.finalPath);
        write_response();
    }
}
int HttpResponse::check_favIco(const RouteResult& routeResult)
{
        std::string path = routeResult.finalPath;
        size_t dotPos = path.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            std::string extension = path.substr(dotPos + 1);
            if (extension == "ico")
                return 1;
        }
    return 0;
}

void HttpResponse::send_large_file(const RouteResult& routeResult)
{
    std::cout << "[Response] Sending large file: " << routeResult.finalPath << " (" << fileSize << " bytes)" << std::endl;
    int fd = open(routeResult.finalPath.c_str(), O_RDONLY);
    setStatusLine();
    setResponseHeaders(routeResult.finalPath);
    std::string headerResponse = status_line;
    for (std::map<std::string, std::string>::const_iterator it = response_headers.begin(); it != response_headers.end(); ++it)
        headerResponse += it->first + ": " + it->second + "\r\n";
    headerResponse += "\r\n";
    send(_clientFd, headerResponse.c_str(), headerResponse.size(), 0);
    char buffer[8192];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t totalSent = 0;
        while (totalSent < bytesRead) {
            ssize_t sent = send(_clientFd, buffer + totalSent, bytesRead - totalSent, 0);
            if (sent == -1) {
                close(fd);
                return;
            }
            totalSent += sent;
        }
    }
    close(fd);
}

void HttpResponse::generateResponse(const HttpRequest& req, RouteResult& routeResult, int clientFd, const std::string& autoIndexContent)
{
    if (check_favIco(routeResult))
        return;
    check_error(req, routeResult);
    if (!errorOccurred)
    {
        _clientFd = clientFd;
        if (req.getMethod() == "GET")
        {
            Status_file(routeResult);
            setStatusLine();
            if (fileSize < 1024 *1024)
                send_small_files(routeResult, autoIndexContent);
            else {
                std::cout << "[Response] Large file detected (" << fileSize << " bytes), using sendfile for efficient transfer." << std::endl;
                send_large_file(routeResult);
            }
        }
    }
    else
    {
        //here i'll implement error responses for 400, 403, 404, 405, 413
        std::cout << "[Response] Error occurred, status code: " << status_code << std::endl;
    }
}

// std::string handleDelete(const RouteResult& route)
// {
//     if (!route.isAllowed)
//     {
//         return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
//     }
//     if (route.isDirectory)
//     {
//         return "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
//     }
//     if (access(route.finalPath.c_str(), F_OK) != 0)
//     {
//         return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
//     }
//     if (access(route.finalPath.c_str(), W_OK) != 0) 
//     {
//         return "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
//     }
//     if (std::remove(route.finalPath.c_str()) == 0)
//     {
//         std::cout << "[DELETE] Successfully removed: " << route.finalPath << std::endl;
//         return "HTTP/1.1 204 No Content\r\nConnection: keep-alive\r\n\r\n";
//     } else
//     {
//         return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
//     }
// }

// std::string handlePost(const HttpRequest& req, const RouteResult& route)
// {
//     if (!route.isAllowed)
//     {
//         return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
//     }
//     if (req.getBody().length() > route.location.maxBodySize)
//     {
//         return "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
//     }
//     std::string targetPath = route.finalPath;
//     struct stat s;
//     if (stat(targetPath.c_str(), &s) == 0 && S_ISDIR(s.st_mode))
//     {
//         if (targetPath[targetPath.length() - 1] != '/')
//         {
//             targetPath += "/";
//         }
//         std::ostringstream filename;
//         filename << "upload_" << time(NULL) << ".bin";
//         targetPath += filename.str();
//     }
//     std::ofstream outFile(targetPath.c_str(), std::ios::binary);
//     if (!outFile.is_open())
//     {
//         std::cerr << "[POST] Error: Could not open " << targetPath << " for writing. Check folder permissions." << std::endl;
//         return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
//     }
//     outFile.write(req.getBody().data(), req.getBody().length());
//     outFile.close();
//     std::cout << "[POST] Successfully created file: " << targetPath << " (" << req.getBody().length() << " bytes)" << std::endl;
//     return "HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
// }