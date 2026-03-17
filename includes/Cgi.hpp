#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "HttpReq.hpp"
#include "Router.hpp"
#include <string>
#include <map>
#include <vector>
#include <sys/types.h>
#include <unistd.h>

class CgiHandler
{
    private:
        std::map<std::string, std::string> env;
        char** envp;
        std::string cgiExecutable;
        std::string scriptPath;

        void initEnv(const HttpRequest& req, const RouteResult& route);
        char** mapToEnvp();
        void freeEnvp();

    public:
        CgiHandler(const std::string& cgiExecutable, const std::string& scriptPath);
        ~CgiHandler();
        pid_t executeCgi(const HttpRequest& req, const RouteResult& route, std::string& outFilename);
};

#endif