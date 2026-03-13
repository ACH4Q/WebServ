#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "Config.hpp"
#include "HttpReq.hpp"
#include <string>

struct RouteResult {
    bool        isAllowed;
    Location    location;
    std::string serverRoot;
    std::map<int, std::string> errorPages;
    std::string finalPath;
    bool        isDirectory; // HADA MACHI DABA
};
// ra7aa hoop
class Router {
    protected:
        static std::string joinPaths(const std::string& path1, const std::string& path2);
        static bool        isDir(const std::string& path);
        static bool        fileExists(const std::string& path);

    public:
        Router();
        ~Router();
        static RouteResult resolve(const HttpRequest& req, const ServerConfig& config);
};

#endif