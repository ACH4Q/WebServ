#include "HttpResponse.hpp"
#include "../includes/Cgi.hpp"
#include "SessionManager.hpp"
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
#include <cerrno>
#include <cstring>

std::map<int, LargeFileTransfer> HttpResponse::_pendingLargeTransfers;

HttpResponse::HttpResponse() : isCgi(false), cgiPid(-1) {}

static void addHeaderToRawResponse(std::string& rawResponse, const std::string& key, const std::string& value)
{
    size_t headerEnd = rawResponse.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return;
    rawResponse.insert(headerEnd, "\r\n" + key + ": " + value);
}

static int extractStatusCodeFromRawResponse(const std::string& rawResponse)
{
    size_t firstSpace = rawResponse.find(' ');
    if (firstSpace == std::string::npos || firstSpace + 4 > rawResponse.size())
        return 0;
    std::string codeText = rawResponse.substr(firstSpace + 1, 3);
    std::stringstream ss(codeText);
    int code = 0;
    ss >> code;
    if (!ss || code < 100 || code > 599)
        return 0;
    return code;
}

LargeFileTransfer::LargeFileTransfer() : fileFd(-1), headerSent(0), bufferLen(0), bufferSent(0), eof(false) {}

#include <cerrno>

bool HttpResponse::hasPendingLargeTransfer(int clientFd)
{
    return _pendingLargeTransfers.find(clientFd) != _pendingLargeTransfers.end();
}

bool HttpResponse::continueLargeTransfer(int clientFd)
{
    std::map<int, LargeFileTransfer>::iterator it = _pendingLargeTransfers.find(clientFd);
    if (it == _pendingLargeTransfers.end())
        return true;

    LargeFileTransfer &transfer = it->second;

    if (transfer.headerSent < transfer.header.size())
    {
        ssize_t sent = send(clientFd, transfer.header.c_str() + transfer.headerSent,
                            transfer.header.size() - transfer.headerSent, MSG_NOSIGNAL);
        if (sent > 0)
            transfer.headerSent += static_cast<size_t>(sent);
        else if (sent == -1)
            return false;
        else
        {
            close(transfer.fileFd);
            _pendingLargeTransfers.erase(it);
            return true;
        }
        return false;
    }

    if (transfer.bufferSent >= transfer.bufferLen && !transfer.eof)
    {
        ssize_t bytesRead = read(transfer.fileFd, transfer.buffer, sizeof(transfer.buffer));
        if (bytesRead > 0)
        {
            transfer.bufferLen = bytesRead;
            transfer.bufferSent = 0;
        }
        else if (bytesRead == 0)
            transfer.eof = true;
        else if (bytesRead == -1)
            return false;
        else
        {
            close(transfer.fileFd);
            _pendingLargeTransfers.erase(it);
            return true;
        }
    }

    if (transfer.bufferSent < transfer.bufferLen)
    {
        ssize_t sent = send(clientFd, transfer.buffer + transfer.bufferSent,
                            transfer.bufferLen - transfer.bufferSent, MSG_NOSIGNAL);
        if (sent > 0)
            transfer.bufferSent += sent;
        else if (sent == -1)
            return false;
        else
        {
            close(transfer.fileFd);
            _pendingLargeTransfers.erase(it);
            return true;
        }
    }

    if (transfer.eof && transfer.bufferSent >= transfer.bufferLen)
    {
        close(transfer.fileFd);
        _pendingLargeTransfers.erase(it);
        return true;
    }

    return false;
}

int HttpResponse::check_status_fourhundred(const HttpRequest& req, const RouteResult& routeResult)
{
    if (req.getErrorCode() != 0)
    {
        errorOccurred = true;
        return req.getErrorCode(); 
    }
    if (!routeResult.isAllowed)
    {
        errorOccurred = true;
        return 405;
    }
    if (routeResult.isDirectory && req.getMethod() == "DELETE")
    {
        errorOccurred = true;
        return 403;
    }
    if (!routeResult.isDirectory)
    {
        if (access(routeResult.finalPath.c_str(), F_OK) != 0) 
        {
            if (req.getMethod() != "POST" || 
                routeResult.finalPath.find(".py") != std::string::npos || routeResult.finalPath.find(".php") != std::string::npos || routeResult.finalPath.find(".sh") != std::string::npos) 
            {
                errorOccurred = true;
                return 404;
            }
        }
        else 
        {
            if (access(routeResult.finalPath.c_str(), R_OK) != 0) 
            {
                 errorOccurred = true;
                 return 403;
            }
        }
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

void HttpResponse::sendErrorPage(const RouteResult& routeResult, int code)
{
    status_code = code;
    setStatusLine();
    std::map<int, std::string>::const_iterator it = routeResult.errorPages.find(code);
    if (it != routeResult.errorPages.end())
    {
        std::string errorFinalPath = routeResult.serverRoot + it->second;
        set_body(errorFinalPath);
        setResponseHeaders(errorFinalPath);
    }
    else
    {
        response_body.clear();
        content_length = "0";
        response_headers["Content-Type"] = "text/plain";
        response_headers["Content-Length"] = content_length;
        response_headers["Connection"] = "close";
        response_headers["Server"] = "webserv";
    }
    write_response();
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
        case 413:
            status_line = "HTTP/1.1 413 Payload Too Large\r\n";
            break;
        case 500:
            status_line = "HTTP/1.1 500 Internal Server Error\r\n";
            break;
        case 501:
            status_line = "HTTP/1.1 501 Not Implemented\r\n";
            break;
        case 502:
            status_line = "HTTP/1.1 502 Bad Gateway\r\n";
            break;
        case 503:
            status_line = "HTTP/1.1 503 Service Unavailable\r\n";
            break;
        default:
            status_line = "HTTP/1.1 500 Internal Server Error\r\n";
            break;
    }
}

void HttpResponse::setHeader(const std::string& key, const std::string& value)
{
    // Generic setter used by SessionManager to attach Set-Cookie.
    response_headers[key] = value;
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
    response_body.clear();
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
    {
        content_length = "0";
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

void HttpResponse::write_response()
{
    std::string finalResponse = status_line;
    for (std::map<std::string, std::string>::const_iterator it = response_headers.begin(); it != response_headers.end(); ++it)
        finalResponse += it->first + ": " + it->second + "\r\n";
    finalResponse += "\r\n" + response_body;
    send(_clientFd, finalResponse.c_str(), finalResponse.size(), MSG_NOSIGNAL);
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
    if (routeResult.isDirectory && routeResult.location.autoindex && access(routeResult.finalPath.c_str(), F_OK) == 0)
    {
        set_directory_autoindex(autoIndexContent);
    }
    else if (routeResult.isDirectory && !routeResult.location.autoindex)
    {
        sendErrorPage(routeResult, 403);
        return;
    }
    else if (routeResult.isDirectory && access(routeResult.finalPath.c_str(), F_OK) != 0)
    {
        sendErrorPage(routeResult, 404);
        return;
    }
    else
    {
        set_body(routeResult.finalPath);
        setResponseHeaders(routeResult.finalPath);
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
    int fd = open(routeResult.finalPath.c_str(), O_RDONLY);
    if (fd == -1)
        return;
    setStatusLine();
    setResponseHeaders(routeResult.finalPath);
    std::string headerResponse = status_line;
    for (std::map<std::string, std::string>::const_iterator it = response_headers.begin(); it != response_headers.end(); ++it)
        headerResponse += it->first + ": " + it->second + "\r\n";
    headerResponse += "\r\n";
    LargeFileTransfer transfer;
    transfer.fileFd = fd;
    transfer.header = headerResponse;
    _pendingLargeTransfers[_clientFd] = transfer;
    continueLargeTransfer(_clientFd);
}

void HttpResponse::generateResponse(const HttpRequest& req, RouteResult& routeResult, int clientFd, const std::string& autoIndexContent)
{
    this->isCgi = false;
    this->cgiPid = -1;
    this->cgiOutFilename = "";
    _clientFd = clientFd;
    response_headers.clear();
    response_body.clear();
    size_t queryPos = routeResult.finalPath.find('?');
    if (queryPos != std::string::npos)
    {
        routeResult.finalPath = routeResult.finalPath.substr(0, queryPos);
    }
    std::string reqPath = req.getPath();
    std::string cleanPath = stripQueryString(reqPath);
    std::string sessionId = extractSessionId(req);
    bool createdNewSession = false;
    
    if (!isSessionValid(sessionId))
    {
        Session& session = createSession();
        sessionId = session.id;
        createdNewSession = true;
        attachSetCookieHeader(*this, sessionId);
    }
    if (cleanPath == "/login")
    {
        std::string username = extractQueryParam(reqPath, "user");
        if (!username.empty() && isSessionValid(sessionId))
        {
            sessions[sessionId].logged_in = true;
            sessions[sessionId].username = username;
            status_code = 302;
            setStatusLine();
            content_length = "0";
            response_headers["Location"] = "/dashboard";
            response_headers["Content-Type"] = "text/plain";
            response_headers["Content-Length"] = content_length;
            response_headers["Connection"] = "close";
            response_headers["Server"] = "webserv";
            write_response();
            return;
        }
    }
    if (routeResult.requires_login)
    {
        if (!isSessionValid(sessionId) || !sessions[sessionId].logged_in)
        {
            status_code = 302;
            setStatusLine();
            content_length = "0";
            response_headers["Location"] = "/login";
            response_headers["Content-Type"] = "text/plain";
            response_headers["Content-Length"] = content_length;
            response_headers["Connection"] = "close";
            response_headers["Server"] = "webserv";
            write_response();
            return;
        }
    }
    if (routeResult.location.is_Redirect)
    {
        status_code = routeResult.location.returnCode;
        setStatusLine();
        content_length = "0";
        response_headers["Location"] = routeResult.location.returnPath;
        response_headers["Connection"] = "close"; // Fixed typo "Conection" here
        response_headers["Content-Length"] = content_length;
        response_headers["Server"] = "webserv";
        write_response();
        return;
    }
    if (check_favIco(routeResult))
        return;
    check_error(req, routeResult);
    if (!errorOccurred)
    {
        if ((req.getMethod() == "GET" || req.getMethod() == "POST") && (routeResult.finalPath.find(".php") != std::string::npos || routeResult.finalPath.find(".py") != std::string::npos || routeResult.finalPath.find(".sh") != std::string::npos))
        {
            handleCgi(req, routeResult);
            return; 
        }
        if (req.getMethod() == "GET")
        {
            Status_file(routeResult);
            setStatusLine();
            if (fileSize < 1024 * 1024)
                send_small_files(routeResult, autoIndexContent);
            else {
                std::ostringstream oss;
                oss << fileSize;
                content_length = oss.str();
                send_large_file(routeResult);
            }
        }
        else if (req.getMethod() == "POST")
        {
            std::string postResponse = handlePost(req, routeResult);
            int postStatus = extractStatusCodeFromRawResponse(postResponse);
            if (postStatus >= 400)
            {
                sendErrorPage(routeResult, postStatus);
                return;
            }
            if (createdNewSession)
                addHeaderToRawResponse(postResponse, "Set-Cookie", "session_id=" + sessionId + "; Path=/");
            send(_clientFd, postResponse.c_str(), postResponse.size(), MSG_NOSIGNAL);
        }
        else if (req.getMethod() == "DELETE")
        {
            std::string deleteResponse = handleDelete(req, routeResult);
            int deleteStatus = extractStatusCodeFromRawResponse(deleteResponse);
            if (deleteStatus >= 400)
            {
                sendErrorPage(routeResult, deleteStatus);
                return;
            }
            if (createdNewSession)
                addHeaderToRawResponse(deleteResponse, "Set-Cookie", "session_id=" + sessionId + "; Path=/");
            send(_clientFd, deleteResponse.c_str(), deleteResponse.size(), MSG_NOSIGNAL);
        }
    }
    else
    {
        sendErrorPage(routeResult, status_code);
        return;
    }
}

std::string HttpResponse::getExtensionFromContentType(const std::string& contentType)
{
    std::string ct = contentType;
    size_t pos = ct.find(';');
    if (pos != std::string::npos)
    {
        ct = ct.substr(0, pos);
    }
    if (ct == "image/jpeg")
        return ".jpg";
    if (ct == "image/png")
        return ".png";
    if (ct == "image/gif")
        return ".gif";
    if (ct == "text/plain")
        return ".txt";
    if (ct == "text/html")
        return ".html";
    if (ct == "application/json")
        return ".json";
    if (ct == "application/pdf")
        return ".pdf";
    return ".bin"; 
}


std::string HttpResponse::handleDelete(const HttpRequest& req, const RouteResult& route)
{
    if (!route.isAllowed)
    {
        return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    
    if (req.getPath().find("../") != std::string::npos)
    {
        return "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
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
    
    if (std::remove(route.finalPath.c_str()) == 0)
    {
        return "HTTP/1.1 204 No Content\r\nConnection: keep-alive\r\n\r\n";
    } 
    else
    {
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
}

std::string HttpResponse::handlePost(const HttpRequest& req, const RouteResult& route)
{
    if (req.getBodyFilename().empty())
    {
        return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    struct stat fileStat;
    if (stat(req.getBodyFilename().c_str(), &fileStat) != 0)
    {
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }

    if ((size_t)fileStat.st_size > route.location.maxBodySize)
    {
        std::remove(req.getBodyFilename().c_str());
        return "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    std::string targetPath = route.finalPath;
    struct stat s;
    bool isDir = (stat(targetPath.c_str(), &s) == 0 && S_ISDIR(s.st_mode));
    if (isDir && targetPath[targetPath.length() - 1] != '/')
    {
        targetPath += "/";
    }
    std::map<std::string, std::string>::const_iterator it = req.getHeaders().find("Content-Type");
    std::string contentType;
    if (it != req.getHeaders().end()) 
    {
        contentType = it->second;
    } 
    else 
    {
        contentType = "";
    }
    bool isMultipart = (contentType.find("multipart/form-data") != std::string::npos);
    if (isMultipart)
    {
        size_t bPos = contentType.find("boundary=");
        if (bPos == std::string::npos)
        {
            return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        }
        std::string boundary = "--" + contentType.substr(bPos + 9);
        std::ifstream inFile(req.getBodyFilename().c_str(), std::ios::binary);
        
        if (!inFile.is_open())
        {
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        }
        size_t fileSize = fileStat.st_size;
        size_t headSize = std::min((size_t)8192, fileSize);
        char headBuf[8192];
        inFile.read(headBuf, headSize);
        std::string headStr(headBuf, headSize);
        size_t startPos = headStr.find(boundary);
        size_t headerEnd = headStr.find("\r\n\r\n", startPos);
        if (startPos == std::string::npos || headerEnd == std::string::npos)
        {
            return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        }
        size_t dataStart = headerEnd + 4;
        std::string filename;
        size_t fnPos = headStr.find("filename=\"", startPos);
        if (fnPos != std::string::npos && fnPos < headerEnd)
        {
            fnPos += 10;
            size_t fnEnd = headStr.find("\"", fnPos);
            filename = headStr.substr(fnPos, fnEnd - fnPos);
        }

        if (filename.empty())
        {
            std::ostringstream tempName;
            tempName << "upload_" << time(NULL) << ".bin";
            filename = tempName.str();
        }

        std::string savePath;
        if (isDir)
        {
            savePath = targetPath + filename;
        }
        else 
        {
            savePath = targetPath; 
        }
        size_t tailSize = std::min((size_t)8192, fileSize);
        inFile.seekg(-tailSize, std::ios::end);
        char tailBuf[8192];
        inFile.read(tailBuf, tailSize);
        std::string tailStr(tailBuf, tailSize);
        size_t endBoundary = tailStr.rfind(boundary);
        if (endBoundary == std::string::npos)
        {
            return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        }
        size_t absoluteEndBoundary = fileSize - tailSize + endBoundary;
        size_t dataEnd = absoluteEndBoundary;
        
        if (dataEnd >= 2) 
        {
            dataEnd -= 2;
        }
        if (dataEnd < dataStart)
        {
            return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        }
        size_t dataLength = dataEnd - dataStart;
        std::ofstream outFile(savePath.c_str(), std::ios::binary);
        if (!outFile.is_open())
        {
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        }
        inFile.seekg(dataStart, std::ios::beg);
        char buffer[8192];
        size_t bytesLeft = dataLength;
        while (bytesLeft > 0)
        {
            size_t chunk = std::min((size_t)8192, bytesLeft);
            inFile.read(buffer, chunk);
            outFile.write(buffer, chunk);
            bytesLeft -= chunk;
        }
        outFile.close();
        inFile.close();
        std::remove(req.getBodyFilename().c_str());
        return "HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    std::string savePath;
    
    if (isDir)
    {
        std::string ext = getExtensionFromContentType(contentType);
        std::ostringstream filename;
        filename << "upload_" << time(NULL) << ext;
        savePath = targetPath + filename.str();
    }
    else
    {
        savePath = targetPath;
    }
    if (std::rename(req.getBodyFilename().c_str(), savePath.c_str()) == 0)
    {
        return "HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
    else
    {
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
    }
}

void HttpResponse::handleCgi(const HttpRequest& req, const RouteResult& routeResult)
{
    std::string cgiExec;
    if (routeResult.finalPath.find(".py") != std::string::npos)
        cgiExec = "/usr/bin/python3";
    else if (routeResult.finalPath.find(".php") != std::string::npos)
        cgiExec = "/usr/bin/php-cgi";
    else if (routeResult.finalPath.find(".sh") != std::string::npos)
        cgiExec = "/bin/bash";
    else
    {
        std::string err = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(_clientFd, err.c_str(), err.length(), 0);
        return;
    }
    CgiHandler cgi(cgiExec, routeResult.finalPath);
    this->cgiPid = cgi.executeCgi(req, routeResult, this->cgiOutFilename);
    this->isCgi = true;
    
    if (this->cgiPid == -1) 
    {
        std::string err = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(_clientFd, err.c_str(), err.length(), 0);
        this->isCgi = false; // Mark as dead
    }
}