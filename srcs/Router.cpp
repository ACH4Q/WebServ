#include "../includes/Router.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

Router::Router() {}
Router::~Router() {}

std::string Router::joinPaths(const std::string& path1, const std::string& path2)
{
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;
    
    bool path1HasSlash = (path1[path1.length() - 1] == '/');
    bool path2HasSlash = (path2[0] == '/');
    
    if (path1HasSlash && path2HasSlash)
        return path1 + path2.substr(1);
    if (!path1HasSlash && !path2HasSlash)
        return path1 + "/" + path2;
        
    return path1 + path2;
}

bool Router::isDir(const std::string& path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
    {
        return S_ISDIR(s.st_mode);
    }
    return false;
}

bool Router::fileExists(const std::string& path)
{
    struct stat s;
    return (stat(path.c_str(), &s) == 0 && !S_ISDIR(s.st_mode));
}

RouteResult Router::resolve(const HttpRequest& req, const ServerConfig& config)
{
    RouteResult result;
    result.isAllowed = true;
    result.isDirectory = false;
    result.serverRoot = config.root;
    result.errorPages = config.errorPages;
    std::string uri = req.getPath();
    result.requires_login = (uri == "/dashboard" || uri.find("/dashboard/") == 0);
    const Location* bestMatch = NULL;
    size_t longestMatchLength = 0;
    
    for (size_t i = 0; i < config.locations.size(); ++i)
    {
        const std::string& locPath = config.locations[i].path;
        if (uri.find(locPath) == 0) {
            if (locPath.length() > longestMatchLength)
            {
                longestMatchLength = locPath.length();
                bestMatch = &config.locations[i];
            }
        }
    }

    if (bestMatch != NULL)
    {
        result.location = *bestMatch;

        bool methodFound = false;
        if (bestMatch->methods.empty()) {
            methodFound = true;
        } else {
            for (size_t i = 0; i < bestMatch->methods.size(); ++i) {
                if (bestMatch->methods[i] == req.getMethod()) {
                    methodFound = true;
                    break;
                }
            }
        }
        if (!methodFound) result.isAllowed = false;
        std::string remainingUri = uri.substr(longestMatchLength);
        if (!bestMatch->alias.empty())
        {
            result.finalPath = joinPaths(bestMatch->alias, remainingUri);
        } else if (!bestMatch->root.empty())
        {
            result.serverRoot = bestMatch->root;
            result.finalPath = joinPaths(bestMatch->root, uri);
        } else
        {
            result.finalPath = joinPaths(config.root, uri);
        }
    } 
    else 
    {
        result.finalPath = joinPaths(config.root, uri);
    }
    if (isDir(result.finalPath))
    {
        result.isDirectory = true;
        if (result.finalPath[result.finalPath.length() - 1] != '/')
        {
            result.finalPath += "/";
        }
        std::string indexFile = ""; 
        if (bestMatch != NULL && !bestMatch->index.empty())
        {
            indexFile = bestMatch->index;
        }
        if (!indexFile.empty()) 
        {
            std::string testPath = result.finalPath + indexFile;

            if (fileExists(testPath))
            {
                result.finalPath = testPath;
                result.isDirectory = false;
            }
        }
    }

    return result;
}