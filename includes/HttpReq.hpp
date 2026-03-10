#ifndef HTTPREQ_HPP
#define HTTPREQ_HPP

#include <map>
#include <string>
#include <vector>

enum RequestParseState
{
    Request_Line,      
    Request_Headers,   
    Request_Body,      
    Request_Chunked,   
    Request_Finished,  
    Request_Error      
};

class HttpRequest
{
    protected:
        std::string                         method; 
        std::string                         path;    
        std::string                         version; 
        std::map<std::string, std::string>  headers; 
        std::string                         bodyFilename; 
        int                                 bodyFd;
        size_t                              bodyBytesWritten;
        RequestParseState                   state;
        size_t                              contentLength;
        std::string                         storage;
        int                                 errorCode;

        bool    parseRequestLine(std::string &line);
        void    parseHeaders(std::string &line);
        void    parseBody();
        void    processChunk(std::string &storage);
        void    openTempFile(); 

    public:
        HttpRequest();
        ~HttpRequest();

        void    parse(std::string &rawBuffer);
        void    reset();
        const std::string&                  getMethod() const;
        const std::string&                  getPath() const;
        const std::string&                  getVersion() const;
        const std::string&                  getBodyFilename() const; 
        const std::map<std::string, std::string>& getHeaders() const;
        RequestParseState                   getState() const;
        int                                 getErrorCode() const;
};

#endif