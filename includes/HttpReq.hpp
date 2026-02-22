#ifndef HTTPREQ_HPP
#define HTTPREQ_HPP

#include <vector>
#include <iostream>
#include <string>
#include <map>

enum RequestParseState
{
    Request_Line,          // GET /images/logo.png HTTP/1.1
    Request_Headers,      // Host: www.example.com User-Agent: Mozilla/5.0
    Request_Body,        // man3rf mohim body
    Request_Chunked,    // data chunked
    Request_Finished   // request valid move on into response
};

class HttpRequest
{
    private:
        std::string                         method;        // GET, POST, DELETE
        std::string                         path;          // /index.html
        std::string                         version;       // HTTP/1.1
        std::map<std::string, std::string>  headers;       // Host -> localhost
        std::string                         body;          // POST data
        RequestParseState                   state;
        size_t                              contentLength; // From Header
        std::string                         storage;       // Buffer for  lines \r\n
        bool    parseRequestLine(std::string &line);
        bool    parseHeaders(std::string &line);
        void    parseBody(std::string &line);
        void    parseChunked(std::string &line);

    public:
        HttpRequest();
        ~HttpRequest();
        bool    parse(const char *data, size_t size);
        std::string getMethod() const;
        std::string getPath() const;
        std::string getHeader(const std::string &key) const;
        std::string getBody() const;
        bool        isFinished() const;
        void        reset();
};

#endif
