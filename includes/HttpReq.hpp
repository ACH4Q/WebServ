#ifndef HTTPREQ_HPP
#define HTTPREQ_HPP

#include <map>
#include <string>
#include <vector>

enum RequestParseState
{
    Request_Line,      // GET /index.html HTTP/1.1
    Request_Headers,   // Host: localhost...
    Request_Body,      // Standard body based on Content-Length
    Request_Chunked,   // Chunked data (required for CGI) 
    Request_Finished,  // complete, ready for response
    Request_Error      // Malformed request or limit exceeded
};

class HttpRequest
{
private:
    std::string                         method;  // GET, POST, or DELETE 
    std::string                         path;    // Requested URI
    std::string                         version; // HTTP protocol version
    std::map<std::string, std::string>  headers; // Key-value pairs (e.g., Content-Type)
    std::string                         body;    // Un-chunked or standard body data
    RequestParseState                   state;
    size_t                              contentLength;
    std::string                         storage;
    int                                 errorCode;
    bool    parseRequestLine(std::string &line);
    void    parseHeaders(std::string &line);
    void    parseBody();
    void    processChunk(std::string &storage);

public:
    HttpRequest();
    ~HttpRequest();
    void    parse(std::string &rawBuffer);
    void    reset();
    const std::string&                  getMethod() const;
    const std::string&                  getPath() const;
    const std::string&                  getVersion() const;
    const std::string&                  getBody() const;
    const std::map<std::string, std::string>& getHeaders() const;
    RequestParseState                   getState() const;
    int                                 getErrorCode() const;

};

#endif