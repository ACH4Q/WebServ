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

struct LargeFileTransfer {
    int fileFd;
    std::string header;
    size_t headerSent;
    ssize_t bufferLen;
    ssize_t bufferSent;
    bool eof;
    char buffer[8192];
    LargeFileTransfer();
};

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
        static std::map<int, LargeFileTransfer> _pendingLargeTransfers;
    public :
        HttpResponse();
        bool isCgi;
        pid_t cgiPid;
        std::string cgiOutFilename;
        void generateResponse(const HttpRequest& req,  RouteResult& routeResult, int clientFd, const std::string& autoIndexContent);
        const std::string& getStatusLine() const;
        const std::map<std::string, std::string>& getResponseHeaders() const;
        const std::string& getResponseBody() const;
        int getStatusCode() const;
        void generate_status_code(const HttpRequest& req, const RouteResult& routeResult);
        void check_error(const HttpRequest& req, const RouteResult& routeResult);
        // Generic header setter used by session helpers (e.g. Set-Cookie).
        void setHeader(const std::string& key, const std::string& value);
        void setStatusLine();
        void setResponseHeaders(std::string path);
        void Status_file(const RouteResult& routeResult);
        void send_small_files(const RouteResult& routeResult, const std::string& autoIndexContent);
        void set_body(const std::string& path);
        void write_response();
        void set_directory_autoindex(const std::string& autoIndexContent);
        int check_favIco(const RouteResult& routeResult);
        void send_large_file(const RouteResult& routeResult);
        static bool hasPendingLargeTransfer(int clientFd);
        static bool continueLargeTransfer(int clientFd);
        std::string handleDelete(const HttpRequest& req, const RouteResult& route);
        std::string handlePost(const HttpRequest& req, const RouteResult& route);
        std::string getExtensionFromContentType(const std::string& contentType);
        int check_status_fourhundred(const HttpRequest& req, const RouteResult& routeResult);
        void sendErrorPage(const RouteResult& routeResult, int code);
        void handleCgi(const HttpRequest& req, const RouteResult& routeResult);
};

#endif