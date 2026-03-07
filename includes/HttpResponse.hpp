#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include "HttpReq.hpp"
#include "Router.hpp"
#include <map>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sendfile.h>

class HttpResponse : public HttpRequest, public Router {
    private:
        std::string status_line;
        int _clientFd;
        std::map<std::string, std::string> response_headers;
        std::string response_body;
        std::string content_type;
        std::string content_length;
        std::string connection;
        int status_code;
        off_t fileSize;
    public :
        void generateResponse(const HttpRequest& req,  RouteResult& routeResult, int clientFd);
        void setStatusLine(int code);
        void setResponseHeaders(std::string path);
        void setResponseBody(std::string path);
        const std::string& getStatusLine() const;
        const std::map<std::string, std::string>& getResponseHeaders() const;
        std::string &find_requested_file(std::string& path);
        const std::string& getResponseBody() const;
        void decideStatus(const HttpRequest& req, const std::string& path, bool serverError);
        void write_response();
};

#endif