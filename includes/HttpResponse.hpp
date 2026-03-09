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
        bool errorOccurred;
    public :
        void generateResponse(const HttpRequest& req,  RouteResult& routeResult, int clientFd, const std::string& autoIndexContent);
        
        const std::string& getStatusLine() const;
        const std::map<std::string, std::string>& getResponseHeaders() const;
        const std::string& getResponseBody() const;
        int getStatusCode() const;
        void generate_status_code(const HttpRequest& req, const RouteResult& routeResult);
        void check_error(const HttpRequest& req, const RouteResult& routeResult);
        int check_status_fourhundred(const HttpRequest& req, const RouteResult& routeResult);
        void setStatusLine();
        void setResponseHeaders(std::string path);
        void Status_file(const RouteResult& routeResult);
        void send_small_files(const RouteResult& routeResult, const std::string& autoIndexContent);
        void set_body(const std::string& path);
        void write_response();
        void set_directory_autoindex(const std::string& autoIndexContent);
        int check_favIco(const RouteResult& routeResult);
        void send_large_file(const RouteResult& routeResult);
};

#endif